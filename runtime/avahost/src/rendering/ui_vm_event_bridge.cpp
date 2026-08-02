#include "ui_vm_event_bridge.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "events/IEvent.h"
#include "events/IEventDispatcher.h"

#include "runtime/runtime_host.h"
#include "ui_vm_state_bridge.h"
#include "known_component_properties.h"
#include "avalang.h"

namespace avahost {

namespace {

void BindComponentRefsNative(AvaVM* vm, avalang::ui::IComponent* comp) {
    if (!comp) return;

    const avalang::ui::PropertyValue* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()) {
        std::string comp_id = idProp->AsString();
        ava_value_t dict = ava_dict_create(vm);

        std::size_t kKnownPropsCount = 0;
        const char* const* kKnownProps = KnownComponentPropertyNames(kKnownPropsCount);
        for (size_t i = 0; i < kKnownPropsCount; ++i) {
            const char* key = kKnownProps[i];
            if (std::string(key) == "id") continue;

            if (const avalang::ui::PropertyValue* prop = comp->GetProperty(key)) {
                ava_value_t val;
                if (prop->Type() == avalang::ui::PropertyType::String) {
                    const std::string& s = prop->AsString();
                    val = ava_string_create(vm, s.c_str(), s.size());
                } else if (prop->Type() == avalang::ui::PropertyType::Number) {
                    val.type = AVA_NUMBER;
                    val.as.n = prop->AsNumber();
                } else if (prop->Type() == avalang::ui::PropertyType::Bool) {
                    val.type = AVA_BOOL;
                    val.as.b = prop->AsBool() ? 1 : 0;
                } else {
                    val = ava_value_t{};
                }
                // ava_dict_set stores the Value as-is and does NOT retain it
                // (see c_api.cpp) -- releasing here would drop a freshly
                // created string/etc back to refcount 0 and free it while
                // the dict still points at it (use-after-free on next read,
                // e.g. in ExportComponentPropsNative). The dict owns the
                // reference created above; do not release it separately.
                ava_dict_set(vm, dict, key, val);
            } else {
                ava_dict_set(vm, dict, key, ava_value_t{});
            }
        }

        ava_set_global(vm, comp_id.c_str(), dict);
        ava_value_release(vm, dict);
    }

    for (avalang::ui::IComponent* child : comp->Children()) {
        BindComponentRefsNative(vm, child);
    }
}

void ExportComponentPropsNative(AvaVM* vm, avalang::ui::IComponent* comp) {
    if (!comp) return;

    const avalang::ui::PropertyValue* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()) {
        std::string comp_id = idProp->AsString();
        ava_value_t dict_val = ava_get_global(vm, comp_id.c_str());
        if (dict_val.type == AVA_DICT) {
            void* entries_ptr = nullptr;
            size_t pair_count = ava_dict_entries(vm, dict_val, &entries_ptr);
            if (entries_ptr) {
                ava_dict_pair_t* pairs = static_cast<ava_dict_pair_t*>(entries_ptr);
                for (size_t i = 0; i < pair_count; ++i) {
                    std::string key(pairs[i].key, pairs[i].key_len);
                    if (key == "id") continue;

                    switch (pairs[i].value.type) {
                        case AVA_STRING: {
                            size_t len = 0;
                            const char* data = ava_string_data(vm, pairs[i].value, &len);
                            comp->SetProperty(key.c_str(), avalang::ui::PropertyValue(std::string(data, len)));
                            break;
                        }
                        case AVA_NUMBER:
                            comp->SetProperty(key.c_str(), avalang::ui::PropertyValue(pairs[i].value.as.n));
                            break;
                        case AVA_BOOL:
                            comp->SetProperty(key.c_str(), avalang::ui::PropertyValue(pairs[i].value.as.b != 0));
                            break;
                        default:
                            break;
                    }
                    // pairs[i].value is a borrowed reference into the dict's
                    // own storage (ava_dict_entries, like every other reader
                    // of it in builtin_dicts.cpp, hands back a pointer/value
                    // the dict itself still owns) -- releasing it here would
                    // drop the dict's own refcount to 0 and free memory the
                    // dict still points at.
                }
            }
        }
        ava_value_release(vm, dict_val);
    }

    for (avalang::ui::IComponent* child : comp->Children()) {
        ExportComponentPropsNative(vm, child);
    }
}

class VmEventHandler final : public avalang::ui::events::IEventHandler {
public:
    VmEventHandler(RuntimeHost& host, VmStateBridge& stateBridge, std::string handlerName,
                   avalang::ui::IComponent* treeRoot = nullptr, AvaVM* vm = nullptr)
        : host_(host), stateBridge_(stateBridge), handlerName_(std::move(handlerName)),
          treeRoot_(treeRoot), vm_(vm) {}

    void OnEvent(avalang::ui::events::IEvent* event) override {
        if (!event) return;

        if (vm_ && treeRoot_) {
            BindComponentRefsNative(vm_, treeRoot_);
        }

        std::string error;
        if (host_.InvokeHandler(handlerName_, error)) {
            if (vm_ && treeRoot_) {
                ExportComponentPropsNative(vm_, treeRoot_);
            }
            stateBridge_.RefreshAll();
        }
    }

private:
    RuntimeHost& host_;
    VmStateBridge& stateBridge_;
    std::string handlerName_;
    avalang::ui::IComponent* treeRoot_;
    AvaVM* vm_;
};

struct EventHandlerKey {
    avalang::ui::ComponentId componentId;
    avalang::ui::events::EventType type;

    bool operator==(const EventHandlerKey& other) const {
        return componentId == other.componentId && type == other.type;
    }
};

struct EventHandlerKeyHash {
    size_t operator()(const EventHandlerKey& key) const {
        return std::hash<avalang::ui::ComponentId>()(key.componentId) ^
               (std::hash<uint8_t>()(static_cast<uint8_t>(key.type)) << 1);
    }
};

std::unordered_map<EventHandlerKey, std::unique_ptr<VmEventHandler>, EventHandlerKeyHash> g_eventHandlers;
std::mutex g_eventHandlersMutex;

const std::unordered_map<std::string, avalang::ui::events::EventType>& EventPropertyMap() {
    static const std::unordered_map<std::string, avalang::ui::events::EventType> map = {
        {"click", avalang::ui::events::EventType::Click},
        {"onmouseenter", avalang::ui::events::EventType::MouseEnter},
        {"onmouseleave", avalang::ui::events::EventType::MouseLeave},
        {"onfocus", avalang::ui::events::EventType::Focus},
        {"onblur", avalang::ui::events::EventType::Blur},
        {"onkeydown", avalang::ui::events::EventType::KeyDown},
        {"onkeyup", avalang::ui::events::EventType::KeyUp},
    };
    return map;
}

void WalkAndWire(avalang::ui::IComponent* node, avalang::ui::IComponent* treeRoot,
                  avalang::ui::events::IEventDispatcher& dispatcher,
                  RuntimeHost& host, VmStateBridge& stateBridge, AvaVM* vm, int& wiredCount) {
    if (!node) return;

    for (const auto& [propertyName, eventType] : EventPropertyMap()) {
        const avalang::ui::PropertyValue* prop = node->GetProperty(propertyName);
        if (!prop || prop->Type() != avalang::ui::PropertyType::String || prop->AsString().empty()) continue;

        auto handler = std::make_unique<VmEventHandler>(host, stateBridge, prop->AsString(), treeRoot, vm);
        avalang::ui::events::IEventHandler* rawHandler = handler.get();

        EventHandlerKey key{node->Id(), eventType};
        {
            std::lock_guard<std::mutex> lock(g_eventHandlersMutex);
            g_eventHandlers[key] = std::move(handler);
        }
        dispatcher.Subscribe(node->Id(), eventType, rawHandler);
        ++wiredCount;
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        WalkAndWire(child, treeRoot, dispatcher, host, stateBridge, vm, wiredCount);
    }
}

}  // namespace

int WireVmEventHandlers(avalang::ui::IComponent* root, avalang::ui::events::IEventDispatcher& dispatcher,
                         RuntimeHost& host, VmStateBridge& stateBridge) {
    int wiredCount = 0;
    AvaVM* vm = host.GetVM();
    WalkAndWire(root, root, dispatcher, host, stateBridge, vm, wiredCount);
    return wiredCount;
}

void BindComponentRefs(RuntimeHost& host, avalang::ui::IComponent* root) {
    AvaVM* vm = host.GetVM();
    if (!vm || !root) return;
    BindComponentRefsNative(vm, root);
}

void ExportComponentProps(RuntimeHost& host, avalang::ui::IComponent* root) {
    AvaVM* vm = host.GetVM();
    if (!vm || !root) return;
    ExportComponentPropsNative(vm, root);
}

}  // namespace avahost

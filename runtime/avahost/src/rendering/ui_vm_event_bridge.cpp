#include "ui_vm_event_bridge.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "events/IEvent.h"
#include "events/IEventDispatcher.h"

#include "runtime/runtime_host.h"
#include "ui_vm_state_bridge.h"
#include "avalang.h"

namespace avahost {

namespace {

// `id` is meant to name a page-unique ref (`ClearCartBtn`, `ConfirmDialog`,
// ...) but nothing enforces that: a reusable component (declares `params`,
// gets instantiated N times -- e.g. ProductCard's internal button) can just
// as easily hardcode a literal `id` on one of its own children, and every
// clone of that component then carries the exact same string. Without a
// guard, every one of those clones would restamp the SAME VM global here,
// so by the time the whole tree is walked the global (and, via
// ExportComponentPropsNative below, every one of those clones' own
// properties) reflects only whichever instance happened to be visited
// last -- silently corrupting every other instance's click handlers,
// text, etc. with data that isn't theirs.
//
// `seenIds` is fresh per top-level Bind/Export call (see BindComponentRefs/
// ExportComponentProps below) and makes this a simple "first one wins"
// rule: the first component encountered with a given `id` claims that
// global exactly as before; any later duplicate is skipped entirely
// rather than clobbering it. A genuinely unique `id` (the normal, intended
// case) is completely unaffected.
void BindComponentRefsNative(AvaVM* vm, avalang::ui::IComponent* comp,
                              std::unordered_set<std::string>& seenIds) {
    if (!comp) return;

    const avalang::ui::PropertyValue* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()) {
        std::string comp_id = idProp->AsString();
        if (seenIds.insert(comp_id).second) {
            ava_value_t dict = ava_dict_create(vm);

            for (const auto& key : comp->PropertyNames()) {
                if (key == "id") continue;

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
                    ava_dict_set(vm, dict, key.c_str(), val);
                } else {
                    ava_dict_set(vm, dict, key.c_str(), ava_value_t{});
                }
            }

            ava_set_global(vm, comp_id.c_str(), dict);
            ava_value_release(vm, dict);
        }
    }

    for (avalang::ui::IComponent* child : comp->Children()) {
        BindComponentRefsNative(vm, child, seenIds);
    }
}

// Mirrors BindComponentRefsNative's "first `id` wins" rule so the two
// stay in lockstep: only the same first-encountered instance that
// actually claimed the global is allowed to read it back and overwrite
// its own properties from it. A later duplicate is left completely
// untouched -- its previously-resolved properties (e.g. a per-instance
// baked `click = OnAddToCart(7)`) survive instead of being overwritten
// with another instance's data.
void ExportComponentPropsNative(AvaVM* vm, avalang::ui::IComponent* comp,
                                 std::unordered_set<std::string>& seenIds) {
    if (!comp) return;

    const avalang::ui::PropertyValue* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()
        && seenIds.insert(idProp->AsString()).second) {
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
                }
            }
        }
        ava_value_release(vm, dict_val);
    }

    for (avalang::ui::IComponent* child : comp->Children()) {
        ExportComponentPropsNative(vm, child, seenIds);
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
            std::unordered_set<std::string> seenIds;
            BindComponentRefsNative(vm_, treeRoot_, seenIds);
        }

        std::string error;
        if (host_.InvokeHandler(handlerName_, error)) {
            if (vm_ && treeRoot_) {
                std::unordered_set<std::string> seenIds;
                ExportComponentPropsNative(vm_, treeRoot_, seenIds);
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
        {"mouseEnter", avalang::ui::events::EventType::MouseEnter},
        {"mouseLeave", avalang::ui::events::EventType::MouseLeave},
        {"focus", avalang::ui::events::EventType::Focus},
        {"blur", avalang::ui::events::EventType::Blur},
        {"keyDown", avalang::ui::events::EventType::KeyDown},
        {"keyUp", avalang::ui::events::EventType::KeyUp},
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
    std::unordered_set<std::string> seenIds;
    BindComponentRefsNative(vm, root, seenIds);
}

void ExportComponentProps(RuntimeHost& host, avalang::ui::IComponent* root) {
    AvaVM* vm = host.GetVM();
    if (!vm || !root) return;
    std::unordered_set<std::string> seenIds;
    ExportComponentPropsNative(vm, root, seenIds);
}

}  // namespace avahost

#include "ui_vm_state_bridge.h"

#include <cctype>
#include <sstream>

#include <nlohmann/json.hpp>

#include "components/PropertyValue.h"
#include "state/PropertyValueEquals.h"

#include "runtime/runtime_host.h"

using nlohmann::json;
using avalang::ui::PropertyType;
using avalang::ui::PropertyValue;

namespace avahost {

namespace {

// Mirrors RuntimeHost::BindState's own anonymous-namespace LooksNumeric
// exactly (digits/one-dot/optional-leading-sign) -- a third copy of the
// same grammar Ava Studio's state_eval.cpp already carries too. Kept
// local rather than exposed from runtime_host.cpp/state_eval.cpp,
// same "no shared internal library across that boundary" note
// runtime_host.cpp's own copy already documents.
bool LooksNumeric(const std::string& v) {
    if (v.empty()) return false;
    size_t i = 0;
    if (v[i] == '+' || v[i] == '-') ++i;
    if (i >= v.size()) return false;
    bool hasDigits = false;
    bool hasDot = false;
    for (; i < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i]))) {
            hasDigits = true;
        } else if (v[i] == '.' && !hasDot) {
            hasDot = true;
        } else {
            return false;
        }
    }
    return hasDigits;
}

// Raw source text (e.g. "0", "true", "Hola") -> PropertyValue, using
// the exact same inference rule RuntimeHost::BindState applies when it
// pushes a `state` block's raw text onto VM globals -- required so a
// VmBackedState's *initial* cached value matches what the VM global
// BindState just set from the same text, without a redundant read-back.
PropertyValue RawTextToPropertyValue(const std::string& raw) {
    if (raw == "true" || raw == "false") {
        return PropertyValue(raw == "true");
    }
    if (LooksNumeric(raw)) {
        return PropertyValue(std::strtod(raw.c_str(), nullptr));
    }
    return PropertyValue(raw);
}

// PropertyValue -> raw text, the inverse of the above and of
// RuntimeHost::ExportStateJson's own per-type formatting (NumberToDisplayString
// et al) -- kept consistent so round-tripping a value through
// BindState/ExportStateJson never drifts from what RawTextToPropertyValue
// would have parsed it back into.
std::string PropertyValueToRawText(const PropertyValue& value) {
    switch (value.Type()) {
        case PropertyType::Bool:
            return value.AsBool() ? "true" : "false";
        case PropertyType::Number: {
            std::ostringstream oss;
            oss << value.AsNumber();
            return oss.str();
        }
        case PropertyType::String:
            return value.AsString();
        case PropertyType::Nil:
        default:
            return "";
    }
}

}  // namespace

// --------------------------- VmBackedState ---------------------------

VmBackedState::VmBackedState(RuntimeHost& host, std::string key, PropertyValue initial)
    : host_(host), key_(std::move(key)), value_(std::move(initial)) {}

const PropertyValue& VmBackedState::Value() const { return value_; }

void VmBackedState::Set(PropertyValue value) {
    // Push through to the VM first (single-key BindState call -- see
    // ui_vm_state_bridge.h's class comment for why this reuses
    // RuntimeHost's existing bulk API instead of a new single-global
    // method), then update the local cache/notify -- same order
    // RuntimeHost::BindState followed by a read would produce.
    json single = json::object();
    single[key_] = PropertyValueToRawText(value);
    host_.BindState(single.dump());
    NotifyIfChanged(value);
}

std::size_t VmBackedState::Subscribe(ChangeHandler handler) {
    std::size_t id = nextSubscriptionId_++;
    subscribers_.emplace_back(id, std::move(handler));
    return id;
}

void VmBackedState::Unsubscribe(std::size_t subscriptionId) {
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
        if (it->first == subscriptionId) {
            subscribers_.erase(it);
            return;
        }
    }
}

void VmBackedState::RefreshFromVm() {
    // ExportStateJson reads this key's *current* VM global back out --
    // exactly what's needed after a handler mutated it directly
    // (`counter = counter + 1`), bypassing this class's Set() entirely.
    json template_ = json::object();
    template_[key_] = PropertyValueToRawText(value_);
    std::string exported = host_.ExportStateJson(template_.dump());

    json parsed;
    try {
        parsed = json::parse(exported);
    } catch (const json::exception&) {
        return;  // malformed -- keep the cache as-is, fail soft
    }
    if (!parsed.is_object() || !parsed.contains(key_)) return;

    const auto& v = parsed[key_];
    const std::string raw = v.is_string() ? v.get<std::string>() : v.dump();
    NotifyIfChanged(RawTextToPropertyValue(raw));
}

void VmBackedState::NotifyIfChanged(const PropertyValue& newValue) {
    if (avalang::ui::state::PropertyValueEquals(value_, newValue)) return;
    value_ = newValue;
    // Snapshot before invoking, same tolerance StateImpl::Set documents
    // for a handler that subscribes/unsubscribes (even itself) as a
    // reaction to this very change.
    auto snapshot = subscribers_;
    for (auto& [id, handler] : snapshot) {
        (void)id;
        if (handler) handler(value_);
    }
}

// --------------------------- VmStateBridge ---------------------------

VmStateBridge::VmStateBridge(RuntimeHost& host) : host_(host) {}

void VmStateBridge::Bind(const std::unordered_map<std::string, std::string>& stateSpec) {
    json spec = json::object();
    for (const auto& [key, raw] : stateSpec) spec[key] = raw;
    templateStateJson_ = spec.dump();
    host_.BindState(templateStateJson_);

    states_.clear();
    states_.reserve(stateSpec.size());
    for (const auto& [key, raw] : stateSpec) {
        states_.emplace_back(key, std::make_unique<VmBackedState>(host_, key, RawTextToPropertyValue(raw)));
    }
}

void VmStateBridge::BindWithOverlay(const std::unordered_map<std::string, std::string>& stateSpec,
                                      const std::string& cachedStateJson) {
    // Fase 20.2.4: cached values win over file defaults for keys that
    // exist in both (mirror of AvaHostApp's stateCache_ merge in
    // web/server/app.cpp, just one layer up so the adapter doesn't
    // need to know about the JSON shape directly).
    json spec = json::object();
    for (const auto& [key, raw] : stateSpec) spec[key] = raw;
    if (!cachedStateJson.empty()) {
        try {
            json cached = json::parse(cachedStateJson);
            if (cached.is_object()) {
                for (auto it = cached.begin(); it != cached.end(); ++it) {
                    if (it.value().is_string()) spec[it.key()] = it.value().get<std::string>();
                    else spec[it.key()] = it.value().dump();
                }
            }
        } catch (const json::exception&) {
            // Malformed cache (shouldn't happen, ExportStateJson only
            // writes valid JSON) -- fall through with file defaults
            // rather than fail the request.
        }
    }
    templateStateJson_ = spec.dump();
    host_.BindState(templateStateJson_);

    states_.clear();
    states_.reserve(spec.size());
    for (auto it = spec.begin(); it != spec.end(); ++it) {
        const std::string raw = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
        states_.emplace_back(it.key(), std::make_unique<VmBackedState>(host_, it.key(), RawTextToPropertyValue(raw)));
    }
}

void VmStateBridge::RefreshAll() {
    for (auto& [key, state] : states_) {
        (void)key;
        state->RefreshFromVm();
    }
}

avalang::ui::IState* VmStateBridge::Find(const std::string& key) const {
    for (const auto& [k, state] : states_) {
        if (k == key) return state.get();
    }
    return nullptr;
}

std::string VmStateBridge::ExportJson() const { return host_.ExportStateJson(templateStateJson_); }

std::string VmStateBridge::EvalIdentifier(const std::string& raw) const {
    return host_.EvalPropertyExpr(raw);
}

}  // namespace avahost

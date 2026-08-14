#include "ui_vm_state_bridge.h"

#include <nlohmann/json.hpp>

#include "components/PropertyValue.h"
#include "parser/AvauiPropertyCoercion.h"
#include "state/PropertyValueEquals.h"

#include "runtime/runtime_host.h"

using nlohmann::json;
using avalang::ui::PropertyType;
using avalang::ui::PropertyValue;
using avalang::ui::parser::InferValue;

namespace avahost {

namespace {

PropertyValue RawTextToPropertyValue(const std::string& raw) {
    return InferValue(raw);
}

std::string PropertyValueToRawText(const PropertyValue& value) {
    switch (value.Type()) {
        case PropertyType::Bool:
            return value.AsBool() ? "true" : "false";
        case PropertyType::Number:
            return avalang::ui::parser::NumberToDisplayString(value.AsNumber());
        case PropertyType::String:
            return value.AsString();
        case PropertyType::Nil:
        default:
            return "";
    }
}

}



VmBackedState::VmBackedState(RuntimeHost& host, std::string key, PropertyValue initial)
    : host_(host), key_(std::move(key)), value_(std::move(initial)) {}

const PropertyValue& VmBackedState::Value() const { return value_; }

void VmBackedState::Set(PropertyValue value) {





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



    json template_ = json::object();
    template_[key_] = PropertyValueToRawText(value_);
    std::string exported = host_.ExportStateJson(template_.dump());

    json parsed;
    try {
        parsed = json::parse(exported);
    } catch (const json::exception&) {
        return;
    }
    if (!parsed.is_object() || !parsed.contains(key_)) return;

    const auto& v = parsed[key_];
    const std::string raw = v.is_string() ? v.get<std::string>() : v.dump();
    NotifyIfChanged(RawTextToPropertyValue(raw));
}

void VmBackedState::NotifyIfChanged(const PropertyValue& newValue) {
    if (avalang::ui::state::PropertyValueEquals(value_, newValue)) return;
    value_ = newValue;



    auto snapshot = subscribers_;
    for (auto& [id, handler] : snapshot) {
        (void)id;
        if (handler) handler(value_);
    }
}



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

}
#include "ui/component.h"

#include <algorithm>

namespace ava {
namespace ui {

void Component::SetProperty(const std::string& key, const Value& value) {
    auto it = std::find_if(properties_.begin(), properties_.end(),
                            [&key](const auto& kv) { return kv.first == key; });
    if (it != properties_.end()) {
        it->second = value;
    } else {
        properties_.emplace_back(key, value);
    }
}

bool Component::HasProperty(const std::string& key) const {
    return std::any_of(properties_.begin(), properties_.end(),
                        [&key](const auto& kv) { return kv.first == key; });
}

Value Component::GetProperty(const std::string& key) const {
    auto it = std::find_if(properties_.begin(), properties_.end(),
                            [&key](const auto& kv) { return kv.first == key; });
    if (it == properties_.end()) return Value::Nil();
    return it->second;
}

void Component::RemoveProperty(const std::string& key) {
    properties_.erase(
        std::remove_if(properties_.begin(), properties_.end(),
                        [&key](const auto& kv) { return kv.first == key; }),
        properties_.end());
}

} // namespace ui
} // namespace ava

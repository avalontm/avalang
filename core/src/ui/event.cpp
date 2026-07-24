#include "ui/component.h"

#include <algorithm>

namespace ava {
namespace ui {

void Component::SetEvent(const std::string& event, const Value& callback) {
    auto it = std::find_if(events_.begin(), events_.end(),
                            [&event](const auto& kv) { return kv.first == event; });
    if (it != events_.end()) {
        it->second = callback;
    } else {
        events_.emplace_back(event, callback);
    }
}

bool Component::HasEvent(const std::string& event) const {
    return std::any_of(events_.begin(), events_.end(),
                        [&event](const auto& kv) { return kv.first == event; });
}

Value Component::GetEvent(const std::string& event) const {
    auto it = std::find_if(events_.begin(), events_.end(),
                            [&event](const auto& kv) { return kv.first == event; });
    if (it == events_.end()) return Value::Nil();
    return it->second;
}

} // namespace ui
} // namespace ava

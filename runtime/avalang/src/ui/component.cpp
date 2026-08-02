#include "ui/component.h"

#include <algorithm>

namespace ava {
namespace ui {

Component::Component(std::string type) : type_(std::move(type)) {}

void Component::AddChild(std::shared_ptr<Component> child) {
    if (!child) return;
    children_.push_back(std::move(child));
}

void Component::RemoveChild(Component* child) {
    if (!child) return;
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
                        [child](const std::shared_ptr<Component>& c) { return c.get() == child; }),
        children_.end());
}

} // namespace ui
} // namespace ava

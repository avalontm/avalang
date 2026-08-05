#include "components/Component.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace components {

Component::Component(ComponentId id, std::string typeName)
    : id_(id), typeName_(std::move(typeName)) {
    node_id_ = "n" + std::to_string(id);
}

ComponentId Component::Id() const {
    return id_;
}

const std::string& Component::NodeId() const {
    return node_id_;
}

const std::string& Component::TypeName() const {
    return typeName_;
}

IComponent* Component::Parent() const {
    return parent_;
}

void Component::SetParent(IComponent* parent) {
    parent_ = parent;
}

void Component::SetProperty(const std::string& name, PropertyValue value) {
    properties_[name] = std::move(value);
}

const PropertyValue* Component::GetProperty(const std::string& name) const {
    auto it = properties_.find(name);
    return it == properties_.end() ? nullptr : &it->second;
}

bool Component::HasProperty(const std::string& name) const {
    return properties_.find(name) != properties_.end();
}

void Component::RemoveProperty(const std::string& name) {
    properties_.erase(name);
}

std::vector<std::string> Component::PropertyNames() const {
    std::vector<std::string> names;
    names.reserve(properties_.size());
    for (const auto& [key, _] : properties_) {
        names.push_back(key);
    }
    return names;
}

std::vector<IComponent*>& Component::MutableSlot(const std::string& slot) {
    for (auto& entry : slots_) {
        if (entry.first == slot) {
            return entry.second;
        }
    }
    slots_.emplace_back(slot, std::vector<IComponent*>{});
    return slots_.back().second;
}

void Component::AddChild(IComponent* child, const std::string& slot) {
    if (!child) {
        return;
    }
    MutableSlot(slot).push_back(child);
    static_cast<Component*>(child)->SetParent(this);
}

void Component::RemoveChild(IComponent* child) {
    if (!child) {
        return;
    }
    for (auto& entry : slots_) {
        auto& children = entry.second;
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it);
            static_cast<Component*>(child)->SetParent(nullptr);
            return;
        }
    }
}

const std::vector<IComponent*>& Component::SlotChildren(const std::string& slot) const {
    static const std::vector<IComponent*> kEmpty;
    for (const auto& entry : slots_) {
        if (entry.first == slot) {
            return entry.second;
        }
    }
    return kEmpty;
}

std::vector<std::string> Component::SlotNames() const {
    std::vector<std::string> names;
    names.reserve(slots_.size());
    for (const auto& entry : slots_) {
        names.push_back(entry.first);
    }
    return names;
}

std::vector<IComponent*> Component::Children() const {
    std::vector<IComponent*> all;
    for (const auto& entry : slots_) {
        all.insert(all.end(), entry.second.begin(), entry.second.end());
    }
    return all;
}

} // namespace components
} // namespace ui
} // namespace avalang

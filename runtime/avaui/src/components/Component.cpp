#include "components/Component.h"

#include <algorithm>
#include <utility>

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
    bool wasMounted = mounted_;
    parent_ = parent;
    mounted_ = (parent != nullptr);

    if (mounted_ && !wasMounted) {
        NotifyMount();
    } else if (!mounted_ && wasMounted) {
        NotifyUnmount();
    }
}

bool Component::IsMounted() const {
    return mounted_;
}

void Component::AddLifecycleObserver(ILifecycleObserver* observer) {
    if (!observer) return;
    lifecycleObservers_.push_back(observer);
}

void Component::RemoveLifecycleObserver(ILifecycleObserver* observer) {
    lifecycleObservers_.erase(
        std::remove(lifecycleObservers_.begin(), lifecycleObservers_.end(), observer),
        lifecycleObservers_.end()
    );
}

void Component::NotifyMount() {
    for (ILifecycleObserver* observer : lifecycleObservers_) {
        observer->OnMount(this);
    }
}

void Component::NotifyUnmount() {
    for (ILifecycleObserver* observer : lifecycleObservers_) {
        observer->OnUnmount(this);
    }
}

unsigned long long Component::Version() const {
    return version_;
}

void Component::TouchVersion() {
    Invalidate(kInvalidationAll);
}

void Component::Invalidate(InvalidationFlag flags) {
    ++version_;
    if (HasInvalidationFlag(flags, InvalidationFlag::Layout)) {
        ++layoutVersion_;
    }
    if (HasInvalidationFlag(flags, InvalidationFlag::Paint)) {
        ++paintVersion_;
    }
    if (HasInvalidationFlag(flags, InvalidationFlag::Scene)) {
        ++sceneVersion_;
    }
    if (parent_) {
        parent_->Invalidate(flags);
    }
}

unsigned long long Component::LayoutVersion() const {
    return layoutVersion_;
}

unsigned long long Component::PaintVersion() const {
    return paintVersion_;
}

unsigned long long Component::SceneVersion() const {
    return sceneVersion_;
}

void Component::SetProperty(const std::string& name, PropertyValue value) {
    SetProperty(name, std::move(value), kInvalidationAll);
}

void Component::SetProperty(const std::string& name, PropertyValue value, InvalidationFlag flags) {
    properties_[name] = std::move(value);
    Invalidate(flags);
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
    TouchVersion();
}

std::vector<std::string> Component::PropertyNames() const {
    std::vector<std::string> names;
    names.reserve(properties_.size());
    for (const auto& entry : properties_) {
        names.push_back(entry.first);
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
    child->SetParent(this);
    TouchVersion();
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
            child->SetParent(nullptr);
            TouchVersion();
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

}
}
}
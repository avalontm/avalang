#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "components/IComponent.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace components {

class Component final : public IComponent, private common::NonCopyable {
public:
    Component(ComponentId id, std::string typeName);

    ComponentId Id() const override;
    const std::string& NodeId() const override;
    const std::string& TypeName() const override;

    IComponent* Parent() const override;
    void SetParent(IComponent* parent) override;

    unsigned long long Version() const override;
    void TouchVersion() override;

    void Invalidate(InvalidationFlag flags) override;
    unsigned long long LayoutVersion() const override;
    unsigned long long PaintVersion() const override;
    unsigned long long SceneVersion() const override;

    void SetProperty(const std::string& name, PropertyValue value) override;
    void SetProperty(const std::string& name, PropertyValue value, InvalidationFlag flags) override;
    const PropertyValue* GetProperty(const std::string& name) const override;
    bool HasProperty(const std::string& name) const override;
    void RemoveProperty(const std::string& name) override;
    std::vector<std::string> PropertyNames() const override;

    void AddChild(IComponent* child, const std::string& slot) override;
    void RemoveChild(IComponent* child) override;
    const std::vector<IComponent*>& SlotChildren(const std::string& slot) const override;
    std::vector<std::string> SlotNames() const override;
    std::vector<IComponent*> Children() const override;

    void AddLifecycleObserver(ILifecycleObserver* observer) override;
    void RemoveLifecycleObserver(ILifecycleObserver* observer) override;
    bool IsMounted() const override;

private:
    ComponentId id_;
    std::string node_id_;
    std::string typeName_;
    IComponent* parent_ = nullptr;
    bool mounted_ = false;
    unsigned long long version_ = 1;
    unsigned long long layoutVersion_ = 1;
    unsigned long long paintVersion_ = 1;
    unsigned long long sceneVersion_ = 1;

    std::unordered_map<std::string, PropertyValue> properties_;

    std::vector<std::pair<std::string, std::vector<IComponent*>>> slots_;
    std::vector<ILifecycleObserver*> lifecycleObservers_;

    std::vector<IComponent*>& MutableSlot(const std::string& slot);
    void NotifyMount();
    void NotifyUnmount();
};

}
}
}
#ifndef AVA_UI_COMPONENTS_COMPONENT_H
#define AVA_UI_COMPONENTS_COMPONENT_H

#include <unordered_map>
#include <utility>
#include <vector>

#include "components/IComponent.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace components {

// Concrete IComponent. Internal -- consumers only ever see IComponent*,
// obtained from ComponentTree. One instance per node; owned exclusively
// by the ComponentTree that created it (see ComponentTreeImpl).
class Component final : public IComponent, private common::NonCopyable {
public:
    Component(ComponentId id, std::string typeName);

    ComponentId Id() const override;
    const std::string& TypeName() const override;

    IComponent* Parent() const override;

    // Not part of IComponent -- only ComponentTreeImpl and Component
    // itself (via AddChild/RemoveChild) reassign parentage.
    void SetParent(IComponent* parent);

    void SetProperty(const std::string& name, PropertyValue value) override;
    const PropertyValue* GetProperty(const std::string& name) const override;
    bool HasProperty(const std::string& name) const override;
    void RemoveProperty(const std::string& name) override;

    void AddChild(IComponent* child, const std::string& slot) override;
    void RemoveChild(IComponent* child) override;
    const std::vector<IComponent*>& SlotChildren(const std::string& slot) const override;
    std::vector<std::string> SlotNames() const override;
    std::vector<IComponent*> Children() const override;

private:
    ComponentId id_;
    std::string typeName_;
    IComponent* parent_ = nullptr;

    std::unordered_map<std::string, PropertyValue> properties_;

    // Ordered slot names (declaration order) + children per slot
    // (insertion order), kept as a single vector of pairs so both
    // orders are preserved without a second lookup structure. UI trees
    // have few slots per component -- a linear scan is fine.
    std::vector<std::pair<std::string, std::vector<IComponent*>>> slots_;

    std::vector<IComponent*>& MutableSlot(const std::string& slot);
};

} // namespace components
} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_COMPONENT_H

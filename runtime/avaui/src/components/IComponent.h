#ifndef AVA_UI_COMPONENTS_ICOMPONENT_H
#define AVA_UI_COMPONENTS_ICOMPONENT_H

#include <string>
#include <vector>

#include "Fwd.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

// Node of the Component Tree (Phase 2). Pure hierarchy + property bag +
// named slots -- no layout, no rendering, no state binding. Those are
// consumed/interpreted by later phases (3, 4, 6); IComponent only
// stores the data they will read.
//
// Instances are owned exclusively by the ComponentTree that created
// them (see ComponentTree::CreateComponent) -- consumers only ever
// hold a non-owning IComponent*.
class IComponent {
public:
    virtual ~IComponent() = default;

    virtual ComponentId Id() const = 0;
    virtual const std::string& NodeId() const = 0;
    virtual const std::string& TypeName() const = 0;

    virtual IComponent* Parent() const = 0;

    // Properties -- generic key/value bag. SetProperty overwrites any
    // existing value for `name`.
    virtual void SetProperty(const std::string& name, PropertyValue value) = 0;
    virtual const PropertyValue* GetProperty(const std::string& name) const = 0;
    virtual bool HasProperty(const std::string& name) const = 0;
    virtual void RemoveProperty(const std::string& name) = 0;
    virtual std::vector<std::string> PropertyNames() const = 0;

    // Slots -- named, ordered child containers. "default" is used when
    // no slot is given. A component may expose zero, one or many slots
    // (e.g. a Page uses only "default"; a future layout control could
    // expose "header"/"footer"/"default"). Declaring/populating a slot
    // is the same operation: the first AddChild for a new slot name
    // creates it, in call order.
    virtual void AddChild(IComponent* child, const std::string& slot = "default") = 0;

    // Removes `child` from whichever slot currently holds it (no-op if
    // not a child of this component). Does not destroy `child`.
    virtual void RemoveChild(IComponent* child) = 0;

    virtual const std::vector<IComponent*>& SlotChildren(const std::string& slot) const = 0;
    virtual std::vector<std::string> SlotNames() const = 0;

    // All children across every slot, in slot-declaration then
    // insertion order. Convenience for consumers that don't care about
    // slots (e.g. a generic tree walker).
    virtual std::vector<IComponent*> Children() const = 0;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_ICOMPONENT_H

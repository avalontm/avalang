#pragma once

#include <string>
#include <vector>

#include "Fwd.h"
#include "components/PropertyValue.h"
#include "components/ILifecycleObserver.h"

namespace avalang {
namespace ui {

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual ComponentId Id() const = 0;
    virtual const std::string& NodeId() const = 0;
    virtual const std::string& TypeName() const = 0;

    virtual IComponent* Parent() const = 0;
    virtual void SetParent(IComponent* parent) = 0;

    virtual unsigned long long Version() const = 0;
    virtual void TouchVersion() = 0;

    virtual void SetProperty(const std::string& name, PropertyValue value) = 0;
    virtual const PropertyValue* GetProperty(const std::string& name) const = 0;
    virtual bool HasProperty(const std::string& name) const = 0;
    virtual void RemoveProperty(const std::string& name) = 0;
    virtual std::vector<std::string> PropertyNames() const = 0;

    virtual void AddChild(IComponent* child, const std::string& slot = "default") = 0;

    virtual void RemoveChild(IComponent* child) = 0;

    virtual const std::vector<IComponent*>& SlotChildren(const std::string& slot) const = 0;
    virtual std::vector<std::string> SlotNames() const = 0;

    virtual std::vector<IComponent*> Children() const = 0;

    virtual void AddLifecycleObserver(ILifecycleObserver* observer) = 0;
    virtual void RemoveLifecycleObserver(ILifecycleObserver* observer) = 0;
    virtual bool IsMounted() const = 0;
};

}
}
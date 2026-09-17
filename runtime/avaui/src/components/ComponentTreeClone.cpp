#include "components/ComponentTreeClone.h"

#include "components/IComponent.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

namespace {

IComponent* CloneNode(const IComponent* source, IComponent* parent, const std::string& slot, ComponentTree* dest) {
    if (!source || !dest) return nullptr;

    IComponent* clone = dest->CreateComponent(source->TypeName());
    if (!clone) return nullptr;

    for (const std::string& name : source->PropertyNames()) {
        if (const PropertyValue* value = source->GetProperty(name)) {
            clone->SetProperty(name, *value);
        }
    }

    if (parent) parent->AddChild(clone, slot);

    for (const std::string& slotName : source->SlotNames()) {
        for (const IComponent* child : source->SlotChildren(slotName)) {
            CloneNode(child, clone, slotName, dest);
        }
    }

    return clone;
}

}

std::unique_ptr<ComponentTree> CloneComponentTree(ComponentTree* source) {
    std::unique_ptr<ComponentTree> dest = ComponentTree::Create();
    if (!source || !source->Root() || !dest) return dest;

    IComponent* clonedRoot = CloneNode(source->Root(), nullptr, "default", dest.get());
    dest->SetRoot(clonedRoot);
    return dest;
}

}
}

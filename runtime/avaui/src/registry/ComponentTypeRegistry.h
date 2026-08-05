#ifndef AVA_UI_REGISTRY_COMPONENTTYPEREGISTRY_H
#define AVA_UI_REGISTRY_COMPONENTTYPEREGISTRY_H

#include <string>
#include <vector>

#include "Export.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace registry {

// Phase 6 (avastudio/avaui unification plan, Part B): component type
// catalog that lives in avaui, the single source of truth. Every real
// control (controls/Xxx.cpp) self-registers here (Phase 7) with its
// canonical TypeName and real defaults -- avastudio (Phase 9) only
// reads this registry, it never keeps its own hand-maintained list.

struct PropertyDefault {
    std::string name;
    PropertyValue value;
};

struct ComponentTypeDescriptor {
    std::string type;          // Real canonical TypeName, e.g. "Button"
    std::string display_name;
    bool is_container = false;
    std::vector<PropertyDefault> default_properties;
};

// Registers a type. Idempotent by `type`: if a descriptor with the same
// `type` already exists, it gets replaced instead of duplicated -- this
// makes it safe to call RegisterComponentType from static initializers
// whose order across translation units isn't guaranteed.
AVA_UI_API void RegisterComponentType(ComponentTypeDescriptor desc);

AVA_UI_API const std::vector<ComponentTypeDescriptor>& GetComponentTypeRegistry();

AVA_UI_API const ComponentTypeDescriptor* FindComponentType(const std::string& type);

} // namespace registry
} // namespace ui
} // namespace avalang

#endif // AVA_UI_REGISTRY_COMPONENTTYPEREGISTRY_H

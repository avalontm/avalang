#pragma once

#include <string>
#include <vector>

#include "designer/component_metadata.h"
#include "designer/types.h"

namespace studio::designer {

class ComponentRegistry {
public:
    static const ComponentRegistry& Instance();

    const std::vector<ComponentMetadata>& All() const;
    const ComponentMetadata* Find(const ComponentTypeId& type) const;
    std::vector<const ComponentMetadata*> ByCategory(const std::string& category) const;
    std::vector<std::string> Categories() const;

private:
    ComponentRegistry();

    std::vector<ComponentMetadata> entries_;
};

}

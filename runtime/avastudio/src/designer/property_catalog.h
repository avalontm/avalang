#pragma once

#include <string>
#include <vector>

#include "designer/property_grid.h"
#include "designer/property_metadata.h"
#include "designer/types.h"

namespace studio::designer {

struct AddablePropertyOption {
    PropertyMetadata metadata;
    std::string initialValue;
    bool declared = false;
};

const PropertyMetadata* FindCatalogProperty(const std::string& name);

PropertyValue ResolveTypedPropertyValue(const UiNode* node, const std::string& key, const std::string& text);

std::vector<AddablePropertyOption> ListAddableProperties(UiNode* node,
                                                          const std::vector<PropertyGridSection>& grid);

}

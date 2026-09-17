#pragma once

#include <string>
#include <vector>

#include "designer/property_metadata.h"
#include "designer/types.h"

namespace studio::designer {

struct PropertyGridRow {
    PropertyMetadata metadata;
    std::string value;
    PropertySource source = PropertySource::Default;
};

struct PropertyGridSection {
    PropertyCategory category;
    std::vector<PropertyGridRow> rows;
};

std::vector<PropertyGridSection> BuildPropertyGrid(UiNode* node);

}

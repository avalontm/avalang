#pragma once

#include <string>
#include <vector>

#include "designer/property_metadata.h"

namespace studio::designer {

struct EventMetadata {
    std::string name;
    std::string description;
};

struct DesignerCapabilities {
    bool isContainer = false;
    bool canReorderChildren = false;
    bool canReparent = true;
    bool canResize = false;
    bool canDelete = true;
    bool canDuplicate = true;
};

struct ComponentMetadata {
    std::string type;
    std::string displayName;
    std::string category;
    std::string icon;
    std::string description;
    int order = 0;
    std::vector<PropertyMetadata> defaultProperties;
    std::vector<PropertyMetadata> supportedProperties;
    std::vector<EventMetadata> supportedEvents;
    std::vector<std::string> allowedParents;
    DesignerCapabilities designerCapabilities;
};

}

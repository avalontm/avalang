#pragma once

#include <string>
#include <vector>

#include "panels/properties_panel.h"



namespace studio::design {












struct ComponentTypeInfo {
    std::string type;
    std::string display_name;
    std::vector<PropertyRow> default_properties;
    bool is_container = false;
    int order = 0;
    std::string category;
    std::string icon;
};








const std::vector<ComponentTypeInfo>& GetComponentCatalog();






const ComponentTypeInfo* FindComponentType(const std::string& type);

}
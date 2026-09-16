#pragma once

#include <string>
#include <vector>

#include "Export.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace registry {

struct PropertyDefault {
    std::string name;
    PropertyValue value;
};

struct ComponentTypeDescriptor {
    std::string type;
    std::string display_name;
    bool is_container = false;
    std::vector<PropertyDefault> default_properties;
};

AVA_UI_API void RegisterComponentType(ComponentTypeDescriptor desc);

AVA_UI_API const std::vector<ComponentTypeDescriptor>& GetComponentTypeRegistry();

AVA_UI_API const ComponentTypeDescriptor* FindComponentType(const std::string& type);

}
}
}
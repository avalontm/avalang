#pragma once

#include <string>

#include "Export.h"
#include "accessibility/AccessibilityNode.h"

namespace avalang {
namespace ui {
namespace accessibility {

struct WindowsMapping {
    std::string controlType;
    std::string automationId;
    std::string ariaRole;
    std::string liveSetting;
};

struct AriaMapping {
    std::string role;
    std::string labelProperty;
    std::string valueProperty;
    std::string checkedProperty;
    std::string selectedProperty;
    std::string disabledProperty;
    std::string expandedProperty;
};

struct AndroidMapping {
    std::string viewClassName;
    std::string role;
    std::string contentDescriptionProperty;
    std::string checkedProperty;
    std::string selectedProperty;
    std::string enabledProperty;
};

struct IosMapping {
    std::string trait;
    std::string labelProperty;
    std::string valueProperty;
    std::string hintProperty;
};

AVA_UI_API WindowsMapping MapRoleToWindows(AccessibilityRole role);
AVA_UI_API AriaMapping MapRoleToAria(AccessibilityRole role);
AVA_UI_API AndroidMapping MapRoleToAndroid(AccessibilityRole role);
AVA_UI_API IosMapping MapRoleToIos(AccessibilityRole role);

AVA_UI_API const char* ActionToString(AccessibilityAction action);
AVA_UI_API const char* RoleToString(AccessibilityRole role);

}
}
}
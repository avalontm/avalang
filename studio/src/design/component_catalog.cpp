#include "design/component_catalog.h"

namespace studio::design {

const std::vector<ComponentTypeInfo>& GetComponentCatalog() {
    static const std::vector<ComponentTypeInfo> catalog = {
        // --- Layout (containers) --------------------------------------
        {"page", "Page", {{"title", "Untitled"}}, /*is_container=*/true},
        {"column", "Column", {}, /*is_container=*/true},
        {"row", "Row", {}, /*is_container=*/true},
        {"stack", "Stack", {}, /*is_container=*/true},
        {"grid", "Grid", {{"columns", "2"}, {"rows", "2"}}, /*is_container=*/true},
        {"flex", "Flex", {}, /*is_container=*/true},

        // --- Content -----------------------------------------------------
        {"text", "Text", {{"value", "Text"}}, /*is_container=*/false},
        {"image", "Image", {{"src", ""}}, /*is_container=*/false},
        {"spacer", "Spacer", {}, /*is_container=*/false},
        {"divider", "Divider", {}, /*is_container=*/false},
        {"link", "Link", {{"text", "Link"}, {"href", ""}}, /*is_container=*/false},

        // --- Interactive ---------------------------------------------------
        {"button", "Button", {{"text", "Button"}, {"enabled", "true"}}, /*is_container=*/false},
        {"textbox", "TextBox", {{"placeholder", ""}, {"value", ""}}, /*is_container=*/false},
        {"checkbox", "CheckBox", {{"label", "CheckBox"}, {"checked", "false"}}, /*is_container=*/false},
        {"radiobutton", "RadioButton", {{"label", "RadioButton"}, {"checked", "false"}}, /*is_container=*/false},
    };
    return catalog;
}

const ComponentTypeInfo* FindComponentType(const std::string& type) {
    for (const auto& info : GetComponentCatalog()) {
        if (info.type == type) return &info;
    }
    return nullptr;
}

} // namespace studio::design

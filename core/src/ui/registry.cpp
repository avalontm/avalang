#include "ui/registry.h"

#include <algorithm>

namespace ava {
namespace ui {

const std::vector<std::string>& ComponentRegistry::KnownTypes() {
    static const std::vector<std::string> types = {
        // Containers
        "Stack", "Grid", "Panel", "Canvas", "Column", "Row", "Flex",
        // Controls
        "Button", "Label", "TextBox", "Image", "CheckBox", "RadioButton",
        // Content
        "Text", "Spacer", "Divider", "Link",
        // Media
        "Video", "Audio",
        // Charts
        "PieChart", "LineChart", "BarChart",
    };
    return types;
}

bool ComponentRegistry::IsKnownType(const std::string& type) {
    const auto& types = KnownTypes();
    return std::find(types.begin(), types.end(), type) != types.end();
}

} // namespace ui
} // namespace ava

#include "controls/ControlMetadata.h"

namespace avalang {
namespace ui {
namespace controls {

namespace {

const std::vector<ControlMetadata>& BuildTable() {
    static const std::vector<ControlMetadata> table = {
        {"Button", "", ControlValueKind::None, "click", ControlActivation::Click},
        {"CheckBox", "isChecked", ControlValueKind::Bool, "change", ControlActivation::Toggle},
        {"RadioButton", "isSelected", ControlValueKind::Bool, "change", ControlActivation::Select},
        {"ComboBox", "selectedValue", ControlValueKind::String, "change", ControlActivation::Popup},
        {"TextBox", "text", ControlValueKind::String, "change", ControlActivation::TextInput},
    };
    return table;
}

}

const std::vector<ControlMetadata>& AllControlMetadata() {
    return BuildTable();
}

const ControlMetadata* FindControlMetadata(const std::string& typeName) {
    for (const auto& entry : AllControlMetadata()) {
        if (entry.typeName == typeName) {
            return &entry;
        }
    }
    return nullptr;
}

}
}
}

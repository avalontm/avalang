#pragma once

#include "Export.h"

#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

enum class ControlValueKind {
    None,
    Bool,
    String,
};

enum class ControlActivation {
    Click,
    Toggle,
    Select,
    Popup,
    TextInput,
};

struct ControlMetadata {
    std::string typeName;
    std::string stateProperty;
    ControlValueKind valueKind;
    std::string eventName;
    ControlActivation activation;
};

AVA_UI_API const std::vector<ControlMetadata>& AllControlMetadata();
AVA_UI_API const ControlMetadata* FindControlMetadata(const std::string& typeName);

}
}
}

#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang {
namespace ui {
namespace controls {

using TextBoxChangeCallback = std::function<void(ComponentId textBoxId, const std::string& newValue)>;

AVA_UI_API IComponent* CreateTextBox(ComponentTree* tree, const std::string& placeholder = "");

AVA_UI_API void SetTextBoxValue(IComponent* textBoxComponent, const std::string& value);

AVA_UI_API std::string GetTextBoxValue(IComponent* textBoxComponent);

AVA_UI_API void BindTextBoxChange(ComponentId textBoxId, TextBoxChangeCallback callback);
AVA_UI_API void UnbindTextBoxChange(ComponentId textBoxId);

}
}
}
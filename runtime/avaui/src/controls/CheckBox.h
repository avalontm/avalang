#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang {
namespace ui {
namespace controls {

using CheckBoxChangeCallback = std::function<void(ComponentId checkBoxId, bool isChecked)>;

AVA_UI_API IComponent* CreateCheckBox(ComponentTree* tree, const std::string& label, bool isChecked = false);

AVA_UI_API void SetCheckBoxChecked(IComponent* checkBoxComponent, bool isChecked);

AVA_UI_API bool ToggleCheckBox(IComponent* checkBoxComponent);

AVA_UI_API bool GetCheckBoxChecked(IComponent* checkBoxComponent);

AVA_UI_API void BindCheckBoxChange(ComponentId checkBoxId, CheckBoxChangeCallback callback);
AVA_UI_API void UnbindCheckBoxChange(ComponentId checkBoxId);

}
}
}
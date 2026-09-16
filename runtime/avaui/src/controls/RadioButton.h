#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang {
namespace ui {
namespace controls {

using RadioButtonChangeCallback = std::function<void(ComponentId radioButtonId, bool isSelected)>;

AVA_UI_API IComponent* CreateRadioButton(ComponentTree* tree, const std::string& label,
                                          const std::string& group, bool isSelected = false);

AVA_UI_API void SelectRadioButton(IComponent* radioButtonComponent);

AVA_UI_API bool GetRadioButtonSelected(IComponent* radioButtonComponent);

AVA_UI_API void BindRadioButtonChange(ComponentId radioButtonId, RadioButtonChangeCallback callback);
AVA_UI_API void UnbindRadioButtonChange(ComponentId radioButtonId);

}
}
}
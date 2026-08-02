#pragma once

#include "components/IComponent.h"
#include "components/ComponentTree.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang::ui::controls {

using ComboBoxChangeCallback = std::function<void(ComponentId comboBoxId, const std::string& newValue)>;

AVA_UI_API IComponent* CreateComboBox(ComponentTree* tree);

AVA_UI_API IComponent* AddOption(ComponentTree* tree, IComponent* comboBoxComponent,
                                  const std::string& value, const std::string& label);

AVA_UI_API void SetSelectedValue(IComponent* comboBoxComponent, const std::string& value);

AVA_UI_API std::string GetSelectedValue(IComponent* comboBoxComponent);

AVA_UI_API std::string GetSelectedLabel(IComponent* comboBoxComponent);

AVA_UI_API void BindComboBoxChange(ComponentId comboBoxId, ComboBoxChangeCallback callback);
AVA_UI_API void UnbindComboBoxChange(ComponentId comboBoxId);

} // namespace avalang::ui::controls

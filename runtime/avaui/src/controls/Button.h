#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <memory>
#include <functional>

namespace avalang {
namespace ui {
namespace controls {

using ButtonClickCallback = std::function<void(ComponentId buttonId)>;

AVA_UI_API IComponent* CreateButton(ComponentTree* tree, const std::string& text);

AVA_UI_API void BindButtonClick(ComponentId buttonId, ButtonClickCallback callback);

AVA_UI_API void UnbindButtonClick(ComponentId buttonId);

}
}
}
#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {
namespace controls {

AVA_UI_API IComponent* CreateText(ComponentTree* tree, const std::string& text);

AVA_UI_API void SetTextValue(IComponent* textComponent, const std::string& text);

}
}
}
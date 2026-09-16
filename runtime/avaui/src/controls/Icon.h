#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {
namespace controls {

AVA_UI_API IComponent* CreateIcon(ComponentTree* tree, const std::string& src);

AVA_UI_API void SetIconSource(IComponent* iconComponent, const std::string& src);

}
}
}

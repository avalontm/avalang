#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {
namespace controls {

AVA_UI_API IComponent* CreateDialog(ComponentTree* tree, const std::string& title);

AVA_UI_API void SetDialogOpen(IComponent* dialog, bool isOpen);

}
}
}
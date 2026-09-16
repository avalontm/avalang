#pragma once

#include "components/IComponent.h"
#include "Export.h"

namespace avalang {
namespace ui {
namespace controls {

AVA_UI_API IComponent* CreateColumn(ComponentTree* tree);
AVA_UI_API IComponent* CreateRow(ComponentTree* tree);
AVA_UI_API IComponent* CreateStack(ComponentTree* tree);

AVA_UI_API void SetContainerSpacing(IComponent* container, double spacingPx);

}
}
}
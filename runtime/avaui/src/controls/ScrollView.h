#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include "Fwd.h"

namespace avalang {
namespace ui {
namespace controls {

AVA_UI_API double GetScrollOffsetX(IComponent* scrollViewComponent);
AVA_UI_API double GetScrollOffsetY(IComponent* scrollViewComponent);

AVA_UI_API void SetScrollOffset(IComponent* scrollViewComponent, double offsetX, double offsetY);

AVA_UI_API bool IsHorizontalScrollView(IComponent* scrollViewComponent);

AVA_UI_API double GetViewportExtent(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal);
AVA_UI_API double GetContentExtent(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal);
AVA_UI_API double GetMaxScrollOffset(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal);

}
}
}

#ifndef AVA_UI_COMPOSITION_COMPOSE_PAGE_WITH_LAYOUT_H
#define AVA_UI_COMPOSITION_COMPOSE_PAGE_WITH_LAYOUT_H

#include <string>

#include "Export.h"
#include "Fwd.h"
#include "layout/LayoutTypes.h"

namespace avalang {
namespace ui {

struct ComposedSlotInfo {
    LayoutRect slotRect;
    bool hasSlot = false;
};

AVA_UI_API ComposedSlotInfo LocateLayoutSlot(ComponentTree* layoutTree,
                                             int viewportWidth,
                                             int viewportHeight);

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPOSITION_COMPOSE_PAGE_WITH_LAYOUT_H

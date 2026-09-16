#pragma once

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

}
}
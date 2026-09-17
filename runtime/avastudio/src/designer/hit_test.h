#pragma once

#include "designer/layout_core.h"
#include "designer/types.h"

namespace studio::designer {

NodeId HitTest(UiComponentTree* tree, const LayoutCore& layout, const LayoutPoint& canvasPoint);

}

#pragma once

#include "design/design_document.h"
#include "designer/types.h"

namespace studio::designer {

studio::design::DropZone ComputeDropZone(const LayoutRect& targetRect, const LayoutPoint& point, bool isContainer);

struct DropIndicator {
    LayoutRect rect;
    bool isLine = false;
};

DropIndicator ComputeDropIndicator(const LayoutRect& targetRect, studio::design::DropZone zone, bool isContainer);

}

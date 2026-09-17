#include "designer/drop_target.h"

#include <algorithm>

namespace studio::designer {

studio::design::DropZone ComputeDropZone(const LayoutRect& targetRect, const LayoutPoint& point, bool isContainer) {
    if (isContainer) {
        return studio::design::DropZone::kInto;
    }
    const double height = std::max(targetRect.height, 1.0);
    const double frac = (point.y - targetRect.y) / height;
    return frac <= 0.5f ? studio::design::DropZone::kBefore : studio::design::DropZone::kAfter;
}

DropIndicator ComputeDropIndicator(const LayoutRect& targetRect, studio::design::DropZone zone, bool isContainer) {
    DropIndicator indicator;

    if (isContainer && zone == studio::design::DropZone::kInto) {
        indicator.rect = targetRect;
        indicator.isLine = false;
        return indicator;
    }

    constexpr double kLineThickness = 3.0;
    indicator.isLine = true;
    const double lineY = (zone == studio::design::DropZone::kBefore)
                              ? targetRect.y - kLineThickness * 0.5
                              : targetRect.y + targetRect.height - kLineThickness * 0.5;
    indicator.rect = LayoutRect{targetRect.x, lineY, targetRect.width, kLineThickness};
    return indicator;
}

}

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "designer/types.h"

namespace studio::designer {

studio::design::DropZone ComputeDropZone(const LayoutRect& targetRect, const LayoutPoint& point, bool isContainer,
                                         bool allowSibling);

struct DropIndicator {
    LayoutRect rect;
    bool isLine = false;
};

DropIndicator ComputeDropIndicator(const LayoutRect& targetRect, studio::design::DropZone zone, bool isContainer);

enum class FlowAxis { kNone, kVertical, kHorizontal, kGrid };

struct FlowLayout {
    FlowAxis axis = FlowAxis::kNone;
    int columns = 1;
};

FlowLayout FlowLayoutOf(const std::string& typeName, bool horizontalDirection, int gridColumns);

size_t ComputeInsertIndex(const FlowLayout& flow, const std::vector<LayoutRect>& childRects, const LayoutPoint& point);

size_t ResolveInsertPosition(const std::vector<std::string>& childIds, size_t index, const std::string& movedId);

bool ComputeInsertMarker(const FlowLayout& flow, const std::vector<LayoutRect>& childRects, size_t index,
                         LayoutRect& out);

}

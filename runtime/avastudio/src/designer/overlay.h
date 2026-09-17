#pragma once

#include <functional>
#include <vector>

#include "designer/layout_core.h"
#include "designer/selection_manager.h"
#include "designer/types.h"

namespace studio::designer {

enum class OverlayItemKind {
    Selection,
    PrimarySelection,
    Hover,
    Guide,
    DropIndicator,
};

struct OverlayItem {
    OverlayItemKind kind;
    NodeId nodeId;
    LayoutRect rect;
    bool compact = false;
};

using OverlayRectResolver = std::function<bool(const NodeId&, OverlayItem&)>;

std::vector<OverlayItem> BuildOverlay(const SelectionManager& selection, const LayoutCore& layout);

std::vector<OverlayItem> BuildOverlay(const SelectionManager& selection, const LayoutCore& layout,
                                       const OverlayRectResolver& resolver);

struct InsetOverlay {
    LayoutRect marginRect;
    bool hasMargin = false;
    LayoutRect paddingRect;
    bool hasPadding = false;
};

InsetOverlay ComputeInsetOverlay(const LayoutRect& rect, const EdgeInsets& padding, const EdgeInsets& margin);

}

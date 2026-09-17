#include "designer/overlay.h"

#include <algorithm>

namespace studio::designer {

namespace {

bool HasAnyInset(const EdgeInsets& insets) {
    return insets.left > 0.0 || insets.top > 0.0 || insets.right > 0.0 || insets.bottom > 0.0;
}

}

std::vector<OverlayItem> BuildOverlay(const SelectionManager& selection, const LayoutCore& layout) {
    return BuildOverlay(selection, layout, OverlayRectResolver());
}

std::vector<OverlayItem> BuildOverlay(const SelectionManager& selection, const LayoutCore& layout,
                                       const OverlayRectResolver& resolver) {
    std::vector<OverlayItem> items;

    const auto push = [&](OverlayItemKind kind, const NodeId& id) {
        OverlayItem item{kind, id, layout.RectOf(id), false};
        if (resolver && !resolver(id, item)) {
            return;
        }
        items.push_back(item);
    };

    const NodeId primary = selection.Primary();
    for (const NodeId& id : selection.Selected()) {
        if (!layout.HasRect(id)) {
            continue;
        }
        push((id == primary) ? OverlayItemKind::PrimarySelection : OverlayItemKind::Selection, id);
    }

    const NodeId& hovered = selection.Hovered();
    if (!hovered.empty() && !selection.IsSelected(hovered) && layout.HasRect(hovered)) {
        push(OverlayItemKind::Hover, hovered);
    }

    return items;
}

InsetOverlay ComputeInsetOverlay(const LayoutRect& rect, const EdgeInsets& padding, const EdgeInsets& margin) {
    InsetOverlay result;

    result.hasMargin = HasAnyInset(margin);
    if (result.hasMargin) {
        result.marginRect = LayoutRect{
            rect.x - margin.left,
            rect.y - margin.top,
            rect.width + margin.left + margin.right,
            rect.height + margin.top + margin.bottom,
        };
    }

    result.hasPadding = HasAnyInset(padding);
    if (result.hasPadding) {
        result.paddingRect = LayoutRect{
            rect.x + padding.left,
            rect.y + padding.top,
            std::max(0.0, rect.width - padding.left - padding.right),
            std::max(0.0, rect.height - padding.top - padding.bottom),
        };
    }

    return result;
}

}

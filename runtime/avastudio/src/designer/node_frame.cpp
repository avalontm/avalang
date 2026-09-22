#include "designer/node_frame.h"

#include <algorithm>

#include "designer/node_kind.h"

namespace studio::designer {

namespace {

LayoutRect FromCorners(double x0, double y0, double x1, double y1) {
    return LayoutRect{x0, y0, x1 - x0, y1 - y0};
}

void CollectRecursive(UiNode* node, const LayoutCore& layout, const LayoutRect& bounds, const ResizeTool* resize,
                      int depth, NodeFrameMap& out) {
    const NodeId id = IdOf(node);
    if (!layout.HasRect(id)) return;

    LayoutRect rect = layout.RectOf(id);
    if (resize != nullptr && resize->IsDragging(id)) {
        if (resize->ResizesX()) rect.width = resize->PreviewWidth();
        if (resize->ResizesY()) rect.height = resize->PreviewHeight();
    }

    const bool isContainer = IsContainerType(node->TypeName());
    NodeFrameParams params;
    params.layout = rect;
    params.isContainer = isContainer;
    params.depth = depth;
    params.skipLeafWireframe = !isContainer;
    params.bounds = bounds;
    out[id] = ComputeNodeFrame(params);

    for (UiNode* child : node->Children()) {
        if (IsDialogNode(child)) continue;
        CollectRecursive(child, layout, bounds, resize, depth + 1, out);
    }
}

}

SelectionBox ComputeSelectionBox(const LayoutRect& base, bool hasOwnPadding) {
    SelectionBox box;
    box.compact = !hasOwnPadding && base.height < kCompactNodeHeightThreshold;
    const double pad = hasOwnPadding ? 0.0 : (box.compact ? kSelectionPadCompact : kSelectionPad);
    box.rect = LayoutRect{base.x - pad, base.y - pad, base.width + pad * 2.0, base.height + pad * 2.0};
    return box;
}

NodeFrame ComputeNodeFrame(const NodeFrameParams& params) {
    NodeFrame frame;

    const double rawX0 = params.layout.x;
    const double rawY0 = params.layout.y + params.offsetY;
    const double rawX1 = rawX0 + params.layout.width;
    const double rawY1 = rawY0 + params.layout.height;
    frame.raw = FromCorners(rawX0, rawY0, rawX1, rawY1);

    const double margin = std::min(kNodeMargin + static_cast<double>(params.depth) * kNodeMarginPerDepth, kNodeMarginMax);
    const double x0 = rawX0 + margin;
    const double y0 = rawY0 + margin;
    const double x1 = std::max(x0, rawX1 - margin);
    const double y1 = std::max(y0, rawY1 - margin);
    frame.content = FromCorners(x0, y0, x1, y1);

    const bool padChrome = params.isContainer && params.depth > 0;
    const double chromeX0 = padChrome ? x0 - kContainerChromePadSide : x0;
    const double chromeY0 = padChrome ? y0 - kContainerChromePadTop : y0;
    const double chromeX1 = padChrome ? x1 + kContainerChromePadSide : x1;
    const double chromeY1 = padChrome ? y1 + kContainerChromePadSide : y1;

    const double minX = params.bounds.x;
    const double minY = params.bounds.y;
    const double maxX = params.bounds.x + params.bounds.width;
    const double maxY = params.bounds.y + params.bounds.height;
    frame.chrome = FromCorners(std::clamp(chromeX0, minX, maxX), std::clamp(chromeY0, minY, maxY),
                                std::clamp(chromeX1, minX, maxX), std::clamp(chromeY1, minY, maxY));

    const LayoutRect& base = params.isContainer ? frame.chrome : (params.skipLeafWireframe ? frame.raw : frame.content);
    frame.base = base;
    const SelectionBox box = ComputeSelectionBox(base, params.isContainer);
    frame.selection = box.rect;
    frame.compact = box.compact;
    return frame;
}

NodeFrameMap CollectNodeFrames(UiNode* root, const LayoutCore& layout, const LayoutRect& bounds,
                                const ResizeTool* resize) {
    NodeFrameMap frames;
    if (root != nullptr) CollectRecursive(root, layout, bounds, resize, 0, frames);
    return frames;
}

}
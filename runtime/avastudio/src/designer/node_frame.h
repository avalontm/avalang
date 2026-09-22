#pragma once

#include <unordered_map>

#include "designer/layout_core.h"
#include "designer/tools.h"
#include "designer/types.h"

namespace studio::designer {

constexpr double kNodeMargin = 3.0;
constexpr double kNodeMarginPerDepth = 1.5;
constexpr double kNodeMarginMax = 12.0;
constexpr double kChipHeight = 15.0;
constexpr double kContainerChromePadSide = kNodeMarginMax + 4.0;
constexpr double kContainerChromePadTop = kChipHeight + kContainerChromePadSide;
constexpr double kSelectionPad = 4.0;
constexpr double kSelectionPadCompact = 2.0;
constexpr double kCompactNodeHeightThreshold = 24.0;

struct SelectionBox {
    LayoutRect rect;
    bool compact = false;
};

SelectionBox ComputeSelectionBox(const LayoutRect& base, bool hasOwnPadding);

struct NodeFrameParams {
    LayoutRect layout;
    bool isContainer = false;
    int depth = 0;
    double offsetY = 0.0;
    bool skipLeafWireframe = false;
    LayoutRect bounds;
};

struct NodeFrame {
    LayoutRect raw;
    LayoutRect content;
    LayoutRect chrome;
    LayoutRect base;
    LayoutRect selection;
    bool compact = false;
};

NodeFrame ComputeNodeFrame(const NodeFrameParams& params);

using NodeFrameMap = std::unordered_map<NodeId, NodeFrame>;

NodeFrameMap CollectNodeFrames(UiNode* root, const LayoutCore& layout, const LayoutRect& bounds,
                                const ResizeTool* resize);

}
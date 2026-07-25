#include "design/layout_engine.h"

#include <algorithm>

namespace studio::design {

const float kDefaultLeafHeight = 32.0f;

namespace {

// Forward-declared: EstimateHeight and LayoutNode are mutually
// recursive -- a "column" container needs each child's height up
// front (to stack them without overlap) before it can hand out y
// positions, and computing a child's height for that means walking
// its own children the same way LayoutNode eventually will for real.
float EstimateHeight(const DesignNode& node, float width);

float EstimateHeight(const DesignNode& node, float width) {
    if (node.children.empty()) return kDefaultLeafHeight;

    if (node.type == "row") {
        // Row splits width across children; its own height is however
        // tall the tallest child ends up (same idea as CSS flex-row
        // align-items: stretch would use, minus the stretch).
        const float child_width = width / static_cast<float>(node.children.size());
        float max_h = 0.0f;
        for (const DesignNode& child : node.children) {
            max_h = std::max(max_h, EstimateHeight(child, child_width));
        }
        return max_h;
    }

    if (node.type == "stack") {
        // Overlapping children all get the full width; height is the
        // tallest one (the others are just shorter within the same box).
        float max_h = 0.0f;
        for (const DesignNode& child : node.children) {
            max_h = std::max(max_h, EstimateHeight(child, width));
        }
        return max_h;
    }

    // "column", "page", "grid", "flex", or any unrecognized container
    // type: fall back to vertical stacking (see the header comment on
    // ComputeLayout for why Grid/Flex land here for now).
    float total = 0.0f;
    for (const DesignNode& child : node.children) {
        total += EstimateHeight(child, width);
    }
    return total;
}

void LayoutNode(const DesignNode& node, Rect rect, LayoutResult& result) {
    result.rects[node.node_uid] = rect;
    if (node.children.empty()) return;

    if (node.type == "row") {
        const float child_width = rect.w / static_cast<float>(node.children.size());
        float x = rect.x;
        for (const DesignNode& child : node.children) {
            Rect child_rect{x, rect.y, child_width, rect.h};
            LayoutNode(child, child_rect, result);
            x += child_width;
        }
        return;
    }

    if (node.type == "stack") {
        for (const DesignNode& child : node.children) {
            LayoutNode(child, rect, result);
        }
        return;
    }

    // Column-like fallback (see EstimateHeight's comment on the same
    // branch): stack children top to bottom, each spanning the full
    // available width and taking exactly the height EstimateHeight
    // already worked out for it.
    float y = rect.y;
    for (const DesignNode& child : node.children) {
        const float child_h = EstimateHeight(child, rect.w);
        Rect child_rect{rect.x, y, rect.w, child_h};
        LayoutNode(child, child_rect, result);
        y += child_h;
    }
}

} // namespace

LayoutResult ComputeLayout(const DesignNode& root, Rect available_space) {
    LayoutResult result;
    LayoutNode(root, available_space, result);
    return result;
}

} // namespace studio::design

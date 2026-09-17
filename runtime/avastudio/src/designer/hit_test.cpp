#include "designer/hit_test.h"

#include <vector>

namespace studio::designer {

namespace {

bool Contains(const LayoutRect& rect, const LayoutPoint& point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
           point.y <= rect.y + rect.height;
}

NodeId HitTestNode(UiNode* node, const LayoutCore& layout, const LayoutPoint& point) {
    if (!node) {
        return NodeId();
    }
    NodeId id = IdOf(node);
    if (!layout.HasRect(id) || !Contains(layout.RectOf(id), point)) {
        return NodeId();
    }

    std::vector<UiNode*> children = node->Children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        NodeId childHit = HitTestNode(*it, layout, point);
        if (!childHit.empty()) {
            return childHit;
        }
    }
    return id;
}

}

NodeId HitTest(UiComponentTree* tree, const LayoutCore& layout, const LayoutPoint& canvasPoint) {
    if (!tree) {
        return NodeId();
    }
    return HitTestNode(tree->Root(), layout, canvasPoint);
}

}

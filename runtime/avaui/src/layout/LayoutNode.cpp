#include "layout/LayoutNode.h"

namespace avalang {
namespace ui {
namespace layout {

LayoutNode::LayoutNode(ComponentId id) : id_(id) {}

ComponentId LayoutNode::Id() const {
    return id_;
}

const LayoutRect& LayoutNode::Rect() const {
    return rect_;
}

ILayoutNode* LayoutNode::Parent() const {
    return parent_;
}

const std::vector<ILayoutNode*>& LayoutNode::Children() const {
    return children_;
}

void LayoutNode::SetRect(const LayoutRect& rect) {
    rect_ = rect;
}

void LayoutNode::SetParent(LayoutNode* parent) {
    parent_ = parent;
}

void LayoutNode::AddChild(LayoutNode* child) {
    children_.push_back(child);
}

} // namespace layout
} // namespace ui
} // namespace avalang

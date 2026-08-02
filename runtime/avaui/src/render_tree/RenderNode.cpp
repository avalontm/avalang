#include "render_tree/RenderNode.h"
#include <algorithm>

namespace avalang {
namespace ui {
namespace render {

RenderNode::RenderNode(ComponentId componentId, RenderNodeType type)
    : componentId_(componentId), type_(type) {
}

void RenderNode::AddChild(std::shared_ptr<IRenderNode> child) {
    if (!child) return;
    // Check for duplicates
    for (const auto& existing : children_) {
        if (existing == child) return;
    }
    children_.push_back(child);
}

void RenderNode::RemoveChild(const std::shared_ptr<IRenderNode>& child) {
    if (!child) return;
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
    }
}

} // namespace render
} // namespace ui
} // namespace avalang

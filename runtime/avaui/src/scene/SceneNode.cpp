#include "scene/SceneNode.h"
#include "scene/Transform.h"
#include <algorithm>

namespace avalang {
namespace ui {
namespace scene {

SceneNode::SceneNode(ComponentId componentId, render::RenderNodeType type,
                     const std::shared_ptr<render::IRenderNode>& renderNode)
    : componentId_(componentId), type_(type), renderNode_(renderNode) {
}

SceneNode::~SceneNode() = default;

void SceneNode::AddChild(std::shared_ptr<ISceneNode> child) {
    if (!child) return;

    for (const auto& existing : children_) {
        if (existing == child) return;
    }

    if (auto childNode = std::dynamic_pointer_cast<SceneNode>(child)) {
        childNode->SetParent(shared_from_this());
    }

    children_.push_back(child);
}

void SceneNode::RemoveChild(const std::shared_ptr<ISceneNode>& child) {
    if (!child) return;
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
    }
}

void SceneNode::SetLocalTransform(const Transform& t) {
    localTransform_ = t;
    MarkDirty();
}

void SceneNode::MarkDirty() {
    dirtyRegion_.isDirty = true;
    if (!renderNode_) {
        return;
    }
    LayoutRect rect = renderNode_->Rect();
    dirtyRegion_.x = static_cast<float>(rect.x);
    dirtyRegion_.y = static_cast<float>(rect.y);
    dirtyRegion_.width = static_cast<float>(rect.width);
    dirtyRegion_.height = static_cast<float>(rect.height);
}

void SceneNode::UpdateWorldTransform(const Transform& parentWorld) {
    worldTransform_.position = parentWorld.position + localTransform_.position;
    worldTransform_.rotation = parentWorld.rotation + localTransform_.rotation;
    worldTransform_.scale = parentWorld.scale * localTransform_.scale;

    for (auto& child : children_) {
        if (auto sceneChild = std::dynamic_pointer_cast<SceneNode>(child)) {
            sceneChild->UpdateWorldTransform(worldTransform_);
        }
    }
}

}
}
}
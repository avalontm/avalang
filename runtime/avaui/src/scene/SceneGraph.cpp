#include "scene/SceneGraph.h"
#include "render_tree/IRenderNode.h"
#include <algorithm>

namespace avalang {
namespace ui {
namespace scene {

SceneGraph::SceneGraph() = default;
SceneGraph::~SceneGraph() = default;

void SceneGraph::Build(const std::shared_ptr<render::IRenderNode>& renderRoot) {
    if (!renderRoot) return;

    nodeMap_.clear();
    dirtyNodes_.clear();
    dirty_ = false;

    root_ = BuildNode(renderRoot);
    if (!root_) return;

    for (auto& pair : nodeMap_) {
        if (auto node = std::dynamic_pointer_cast<SceneNode>(pair.second)) {
            node->MarkDirty();
        }
    }
}

std::shared_ptr<ISceneNode> SceneGraph::BuildNode(const std::shared_ptr<render::IRenderNode>& renderNode) {
    if (!renderNode) return nullptr;

    auto sceneNode = std::make_shared<SceneNode>(
        renderNode->Id(),
        renderNode->Type(),
        renderNode
    );

    nodeMap_[renderNode->Id()] = sceneNode;

    for (const auto& child : renderNode->Children()) {
        BuildNodeRecursive(child, sceneNode);
    }

    return sceneNode;
}

void SceneGraph::BuildNodeRecursive(const std::shared_ptr<render::IRenderNode>& renderNode,
                                    std::shared_ptr<SceneNode> sceneParent) {
    if (!renderNode || !sceneParent) return;

    auto sceneNode = std::make_shared<SceneNode>(
        renderNode->Id(),
        renderNode->Type(),
        renderNode
    );

    nodeMap_[renderNode->Id()] = sceneNode;
    sceneParent->AddChild(sceneNode);

    for (const auto& child : renderNode->Children()) {
        BuildNodeRecursive(child, sceneNode);
    }
}

std::shared_ptr<ISceneNode> SceneGraph::FindNode(ComponentId componentId) const {
    auto it = nodeMap_.find(componentId);
    if (it != nodeMap_.end()) {
        return it->second;
    }
    return nullptr;
}

void SceneGraph::UpdateTransforms() {
    if (!root_) return;

    Transform identity;
    if (auto sceneRoot = std::dynamic_pointer_cast<SceneNode>(root_)) {
        sceneRoot->UpdateWorldTransform(identity);
    }

    dirty_ = false;
}

void SceneGraph::ComputeDirtyRegions() {
    if (!root_) return;

    dirtyNodes_.clear();
    Transform identity;
    ComputeDirtyRegionsRecursive(root_, identity);
}

void SceneGraph::ComputeDirtyRegionsRecursive(const std::shared_ptr<ISceneNode>& node,
                                              const Transform& parentTransform) {
    if (!node) return;

    auto sceneNode = std::dynamic_pointer_cast<SceneNode>(node);
    if (!sceneNode) return;

    sceneNode->UpdateWorldTransform(parentTransform);

    if (sceneNode->GetDirtyRegion().isDirty) {
        dirtyNodes_.push_back(node);
    }

    for (const auto& child : node->Children()) {
        ComputeDirtyRegionsRecursive(child, sceneNode->WorldTransform());
    }
}

void SceneGraph::ForEachInRenderOrder(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) {
    if (!root_) return;
    ForEachInRenderOrderRecursive(root_, visitor);
}

void SceneGraph::ForEachInRenderOrderRecursive(const std::shared_ptr<ISceneNode>& node,
                                               std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) {
    if (!node) return;

    auto children = node->Children();
    std::sort(children.begin(), children.end(),
              [](const auto& a, const auto& b) {
                  return a->ZOrder() < b->ZOrder();
              });

    visitor(node);

    for (const auto& child : children) {
        ForEachInRenderOrderRecursive(child, visitor);
    }
}

void SceneGraph::ForEachDirtyNode(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) {
    for (const auto& node : dirtyNodes_) {
        visitor(node);
    }
}

std::vector<DirtyRegion> SceneGraph::CollectDirtyRects() {
    ComputeDirtyRegions();

    std::vector<DirtyRegion> rects;
    rects.reserve(dirtyNodes_.size());

    for (const auto& node : dirtyNodes_) {
        auto sceneNode = std::dynamic_pointer_cast<SceneNode>(node);
        if (!sceneNode) continue;

        DirtyRegion region = sceneNode->GetDirtyRegion();
        Transform world = sceneNode->WorldTransform();
        region.x += world.position.x;
        region.y += world.position.y;
        rects.push_back(region);
    }

    return rects;
}

}
}
}
#ifndef AVA_UI_SCENE_SCENE_GRAPH_H
#define AVA_UI_SCENE_SCENE_GRAPH_H

#include "scene/ISceneGraph.h"
#include "scene/SceneNode.h"
#include <unordered_map>
#include <vector>

namespace avalang {
namespace ui {
namespace scene {

class SceneGraph : public ISceneGraph {
public:
    SceneGraph();
    ~SceneGraph();

    void Build(const std::shared_ptr<render::IRenderNode>& renderRoot) override;

    std::shared_ptr<ISceneNode> Root() const override { return root_; }
    std::shared_ptr<ISceneNode> FindNode(ComponentId componentId) const override;

    void UpdateTransforms() override;
    void ComputeDirtyRegions() override;

    void ForEachInRenderOrder(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) override;
    void ForEachDirtyNode(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) override;

    void Invalidate() override { dirty_ = true; }
    bool IsDirty() const override { return dirty_; }

private:
    std::shared_ptr<ISceneNode> BuildNode(const std::shared_ptr<render::IRenderNode>& renderNode);
    void BuildNodeRecursive(const std::shared_ptr<render::IRenderNode>& renderNode,
                           std::shared_ptr<SceneNode> sceneParent);

    void ForEachInRenderOrderRecursive(const std::shared_ptr<ISceneNode>& node,
                                       std::function<void(const std::shared_ptr<ISceneNode>&)> visitor);

    void ComputeDirtyRegionsRecursive(const std::shared_ptr<ISceneNode>& node,
                                      const Transform& parentTransform);

    std::shared_ptr<ISceneNode> root_;
    std::unordered_map<ComponentId, std::shared_ptr<ISceneNode>> nodeMap_;
    std::vector<std::shared_ptr<ISceneNode>> dirtyNodes_;
    bool dirty_ = true;
};

} // namespace scene
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SCENE_SCENE_GRAPH_H

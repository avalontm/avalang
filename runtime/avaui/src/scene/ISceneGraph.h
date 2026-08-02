#ifndef AVA_UI_SCENE_ISCENE_GRAPH_H
#define AVA_UI_SCENE_ISCENE_GRAPH_H

#include "scene/ISceneNode.h"
#include "Export.h"
#include <memory>
#include <vector>
#include <functional>

namespace avalang {
namespace ui {

namespace render {
class IRenderNode;
}

namespace scene {

// Scene graph manager — converts render tree into scene hierarchy with transforms, visibility, etc.
class AVA_UI_API ISceneGraph {
public:
    virtual ~ISceneGraph() = default;

    // Factory
    static ISceneGraph* Create();

    // Build scene graph from render tree.
    virtual void Build(const std::shared_ptr<render::IRenderNode>& renderRoot) = 0;

    // Access
    virtual std::shared_ptr<ISceneNode> Root() const = 0;
    virtual std::shared_ptr<ISceneNode> FindNode(ComponentId componentId) const = 0;

    // Update world transforms (call after any local transform changes)
    virtual void UpdateTransforms() = 0;

    // Compute dirty regions (incremental rendering support)
    virtual void ComputeDirtyRegions() = 0;

    // Iterate nodes in render order (sorted by z-order, parent-before-children)
    virtual void ForEachInRenderOrder(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) = 0;

    // Iterate only dirty nodes
    virtual void ForEachDirtyNode(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) = 0;

    // Invalidate entire scene (force full rebuild on next UpdateTransforms)
    virtual void Invalidate() = 0;
    virtual bool IsDirty() const = 0;
};

} // namespace scene
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SCENE_ISCENE_GRAPH_H

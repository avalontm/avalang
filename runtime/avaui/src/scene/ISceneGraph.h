#pragma once

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

class AVA_UI_API ISceneGraph {
public:
    virtual ~ISceneGraph() = default;

    static ISceneGraph* Create();

    virtual void Build(const std::shared_ptr<render::IRenderNode>& renderRoot) = 0;

    virtual std::shared_ptr<ISceneNode> Root() const = 0;
    virtual std::shared_ptr<ISceneNode> FindNode(ComponentId componentId) const = 0;

    virtual void UpdateTransforms() = 0;

    virtual void ComputeDirtyRegions() = 0;

    virtual void ForEachInRenderOrder(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) = 0;

    virtual void ForEachDirtyNode(std::function<void(const std::shared_ptr<ISceneNode>&)> visitor) = 0;

    virtual std::vector<DirtyRegion> CollectDirtyRects() = 0;

    virtual void Invalidate() = 0;
    virtual bool IsDirty() const = 0;
};

}
}
}
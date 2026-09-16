#pragma once

#include "render_tree/IRenderNode.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace avalang {
namespace ui {
namespace scene {

struct Transform {
    glm::vec2 position = {0.0f, 0.0f};
    glm::vec2 scale = {1.0f, 1.0f};
    float rotation = 0.0f;

    glm::mat4 ToMatrix() const;
};

struct ClipRect {
    float x, y, width, height;
    bool enabled = false;
};

struct DirtyRegion {
    float x, y, width, height;
    bool isDirty = false;
};

class ISceneNode {
public:
    virtual ~ISceneNode() = default;

    virtual ComponentId Id() const = 0;
    virtual render::RenderNodeType Type() const = 0;

    virtual const std::vector<std::shared_ptr<ISceneNode>>& Children() const = 0;
    virtual void AddChild(std::shared_ptr<ISceneNode> child) = 0;
    virtual void RemoveChild(const std::shared_ptr<ISceneNode>& child) = 0;
    virtual std::shared_ptr<ISceneNode> Parent() const = 0;

    virtual Transform LocalTransform() const = 0;
    virtual void SetLocalTransform(const Transform& t) = 0;

    virtual Transform WorldTransform() const = 0;

    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool v) = 0;

    virtual float Opacity() const = 0;
    virtual void SetOpacity(float op) = 0;

    virtual int ZOrder() const = 0;
    virtual void SetZOrder(int z) = 0;

    virtual ClipRect ClipBounds() const = 0;
    virtual void SetClipBounds(const ClipRect& clip) = 0;
    virtual bool IsClipped() const = 0;

    virtual DirtyRegion GetDirtyRegion() const = 0;
    virtual void SetDirtyRegion(const DirtyRegion& region) = 0;
    virtual void ClearDirtyRegion() = 0;
    virtual void MarkDirty() = 0;

    virtual const render::IRenderNode* GetRenderNode() const = 0;
};

}
}
}
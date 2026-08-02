#ifndef AVA_UI_SCENE_ISCENE_NODE_H
#define AVA_UI_SCENE_ISCENE_NODE_H

#include "render_tree/IRenderNode.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace avalang {
namespace ui {
namespace scene {

// 2D Transform
struct Transform {
    glm::vec2 position = {0.0f, 0.0f};
    glm::vec2 scale = {1.0f, 1.0f};
    float rotation = 0.0f;  // radians

    glm::mat4 ToMatrix() const;
};

// Clipping bounds (local to parent)
struct ClipRect {
    float x, y, width, height;
    bool enabled = false;
};

// Dirty region tracking
struct DirtyRegion {
    float x, y, width, height;
    bool isDirty = false;
};

// Scene node — enriches render node with transform, visibility, opacity, z-order, clipping.
class ISceneNode {
public:
    virtual ~ISceneNode() = default;

    // Identity
    virtual ComponentId Id() const = 0;
    virtual render::RenderNodeType Type() const = 0;

    // Hierarchy
    virtual const std::vector<std::shared_ptr<ISceneNode>>& Children() const = 0;
    virtual void AddChild(std::shared_ptr<ISceneNode> child) = 0;
    virtual void RemoveChild(const std::shared_ptr<ISceneNode>& child) = 0;
    virtual std::shared_ptr<ISceneNode> Parent() const = 0;

    // Transform (local to parent)
    virtual Transform LocalTransform() const = 0;
    virtual void SetLocalTransform(const Transform& t) = 0;

    // World transform (computed from parent hierarchy)
    virtual Transform WorldTransform() const = 0;

    // Visibility
    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool v) = 0;

    // Opacity (0.0 = fully transparent, 1.0 = fully opaque)
    virtual float Opacity() const = 0;
    virtual void SetOpacity(float op) = 0;

    // Z-order (for painter's algorithm, higher = on top)
    virtual int ZOrder() const = 0;
    virtual void SetZOrder(int z) = 0;

    // Clipping
    virtual ClipRect ClipBounds() const = 0;
    virtual void SetClipBounds(const ClipRect& clip) = 0;
    virtual bool IsClipped() const = 0;

    // Dirty region tracking (for incremental rendering)
    virtual DirtyRegion GetDirtyRegion() const = 0;
    virtual void SetDirtyRegion(const DirtyRegion& region) = 0;
    virtual void ClearDirtyRegion() = 0;
    virtual void MarkDirty() = 0;

    // Linked render node (read-only reference)
    virtual const render::IRenderNode* GetRenderNode() const = 0;
};

} // namespace scene
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SCENE_ISCENE_NODE_H

#ifndef AVA_UI_SCENE_SCENE_NODE_H
#define AVA_UI_SCENE_SCENE_NODE_H

#include "scene/ISceneNode.h"
#include "render_tree/IRenderNode.h"
#include <memory>

namespace avalang {
namespace ui {
namespace scene {

class SceneNode : public ISceneNode, public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(ComponentId componentId, render::RenderNodeType type,
              const std::shared_ptr<render::IRenderNode>& renderNode);
    ~SceneNode();

    // Non-copyable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    ComponentId Id() const override { return componentId_; }
    render::RenderNodeType Type() const override { return type_; }

    const std::vector<std::shared_ptr<ISceneNode>>& Children() const override { return children_; }
    void AddChild(std::shared_ptr<ISceneNode> child) override;
    void RemoveChild(const std::shared_ptr<ISceneNode>& child) override;
    std::shared_ptr<ISceneNode> Parent() const override { return parent_.lock(); }

    Transform LocalTransform() const override { return localTransform_; }
    void SetLocalTransform(const Transform& t) override;

    Transform WorldTransform() const override { return worldTransform_; }

    bool IsVisible() const override { return visible_; }
    void SetVisible(bool v) override { visible_ = v; }

    float Opacity() const override { return opacity_; }
    void SetOpacity(float op) override { opacity_ = glm::clamp(op, 0.0f, 1.0f); }

    int ZOrder() const override { return zOrder_; }
    void SetZOrder(int z) override { zOrder_ = z; }

    ClipRect ClipBounds() const override { return clipBounds_; }
    void SetClipBounds(const ClipRect& clip) override { clipBounds_ = clip; }
    bool IsClipped() const override { return clipBounds_.enabled; }

    DirtyRegion GetDirtyRegion() const override { return dirtyRegion_; }
    void SetDirtyRegion(const DirtyRegion& region) override { dirtyRegion_ = region; }
    void ClearDirtyRegion() override { dirtyRegion_.isDirty = false; }
    void MarkDirty() override { dirtyRegion_.isDirty = true; }

    const render::IRenderNode* GetRenderNode() const override { return renderNode_.get(); }

    // Internal: set parent (called by parent during AddChild)
    void SetParent(std::shared_ptr<ISceneNode> parent) { parent_ = parent; }

    // Internal: update world transform from parent
    void UpdateWorldTransform(const Transform& parentWorld);

private:
    avalang::ui::ComponentId componentId_;
    render::RenderNodeType type_;
    std::shared_ptr<render::IRenderNode> renderNode_;

    std::vector<std::shared_ptr<ISceneNode>> children_;
    std::weak_ptr<ISceneNode> parent_;

    Transform localTransform_;
    Transform worldTransform_;

    bool visible_ = true;
    float opacity_ = 1.0f;
    int zOrder_ = 0;

    ClipRect clipBounds_ = {0, 0, 0, 0, false};
    DirtyRegion dirtyRegion_;
};

} // namespace scene
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SCENE_SCENE_NODE_H

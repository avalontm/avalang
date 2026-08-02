#ifndef AVA_UI_LAYOUT_LAYOUTNODE_H
#define AVA_UI_LAYOUT_LAYOUTNODE_H

#include <vector>

#include "layout/ILayoutNode.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace layout {

// Concrete ILayoutNode. Internal -- consumers only ever see
// ILayoutNode*, obtained from LayoutEngine. Built fresh by
// LayoutEngineImpl on every Compute() call (see
// LayoutEngineImpl::BuildTree); one instance per component in the
// computed subtree, owned by LayoutEngineImpl for the lifetime of
// that result.
class LayoutNode final : public ILayoutNode, private common::NonCopyable {
public:
    explicit LayoutNode(ComponentId id);

    ComponentId Id() const override;
    const LayoutRect& Rect() const override;
    ILayoutNode* Parent() const override;
    const std::vector<ILayoutNode*>& Children() const override;

    // Not part of ILayoutNode -- only LayoutEngineImpl assembles the
    // tree shape (BuildTree) and writes the computed geometry
    // (LayoutEngineImpl::PlaceComponent).
    void SetRect(const LayoutRect& rect);
    void SetParent(LayoutNode* parent);
    void AddChild(LayoutNode* child);

private:
    ComponentId id_;
    LayoutRect rect_;
    LayoutNode* parent_ = nullptr;
    std::vector<ILayoutNode*> children_;
};

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_LAYOUTNODE_H

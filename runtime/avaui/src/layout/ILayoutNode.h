#ifndef AVA_UI_LAYOUT_ILAYOUTNODE_H
#define AVA_UI_LAYOUT_ILAYOUTNODE_H

#include <vector>

#include "Fwd.h"
#include "LayoutTypes.h"

namespace avalang {
namespace ui {

// Node of a Layout Result (Phase 3). Mirrors the Component Tree
// (Phase 2) one-to-one -- same ComponentId, same parent/child shape --
// but only carries the geometry LayoutEngine computed for it. No
// properties, no slots, no rendering: those belong to IComponent
// (Phase 2) and the Render Tree (Phase 6+) respectively.
//
// Owned exclusively by the LayoutEngine that computed it, same
// ownership convention as IComponent/ComponentTree: consumers only
// ever hold a non-owning ILayoutNode*, valid until the engine's next
// Compute() call or destruction.
class ILayoutNode {
public:
    virtual ~ILayoutNode() = default;

    // Identifier of the IComponent this node was computed for. Use
    // this to cross-reference back into the ComponentTree that
    // produced the component passed to LayoutEngine::Compute().
    virtual ComponentId Id() const = 0;

    // Final geometry: this component's own border box. Margin has
    // already been consumed by the parent's arrangement and is not
    // reflected here.
    virtual const LayoutRect& Rect() const = 0;

    virtual ILayoutNode* Parent() const = 0;

    // Children in the same order as the source IComponent's
    // Children() (slot-declaration then insertion order).
    virtual const std::vector<ILayoutNode*>& Children() const = 0;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_ILAYOUTNODE_H

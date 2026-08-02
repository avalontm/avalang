#ifndef AVA_UI_LAYOUT_LAYOUTENGINE_H
#define AVA_UI_LAYOUT_LAYOUTENGINE_H

#include <memory>

#include "Export.h"
#include "Fwd.h"
#include "layout/ILayoutNode.h"

namespace avalang {
namespace ui {

// Phase 3 -- Layout Engine. Walks a Component Tree (Phase 2, see
// IComponent/ComponentTree) and computes a position and size for
// every component: width, height, margin, padding, alignment, and
// Row/Column/Stack arrangement (see
// docs/AVALANG_UI_IMPLEMENTATION_PLAN.md, "Phase 3"). Geometry is read
// entirely from IComponent's generic property bag and TypeName() --
// the recognized property names are documented in
// src/layout/LayoutProperties.h (internal, but the names are the
// public contract .avaui authors/consumers rely on).
//
// TypeName() drives the arrangement algorithm: "Row" lays children
// out horizontally, "Column" vertically, and every other TypeName
// (including "Stack" and any component the engine doesn't otherwise
// recognize -- Page, Container, Text, Button, ...) falls back to
// Stack's overlay behavior. Control-specific layout behavior belongs
// to a later phase (controls/); Phase 3 only needs a sane default for
// "a container with children".
//
// The Layout Engine never draws and never touches platform/render
// state -- its only output is geometry (LayoutRect via ILayoutNode),
// consumed later by the Render Tree (Phase 6+).
class AVA_UI_API LayoutEngine {
public:
    static std::unique_ptr<LayoutEngine> Create();

    virtual ~LayoutEngine() = default;

    // Computes layout for `componentRoot` and its whole subtree, given
    // `available` -- the space the caller offers the root (e.g. a
    // window's client area). Returns the resulting root ILayoutNode,
    // owned by this LayoutEngine until the next Compute() call or
    // destruction. Recomputing replaces the previous result entirely --
    // this phase has no incremental/partial relayout. Returns nullptr
    // if `componentRoot` is null.
    virtual ILayoutNode* Compute(IComponent* componentRoot, const LayoutRect& available) = 0;

    // Lookup into the most recent Compute() result by ComponentId.
    // Returns nullptr if Compute() has not been called, or if `id`
    // was not part of the computed subtree.
    virtual ILayoutNode* FindNode(ComponentId id) const = 0;

    // The root of the most recent Compute() result, or nullptr if
    // Compute() has not been called (or was last called with a null
    // componentRoot).
    virtual ILayoutNode* Root() const = 0;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_LAYOUTENGINE_H

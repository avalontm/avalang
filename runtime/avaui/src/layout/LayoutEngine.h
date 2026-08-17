#ifndef AVA_UI_LAYOUT_LAYOUTENGINE_H
#define AVA_UI_LAYOUT_LAYOUTENGINE_H

#include <functional>
#include <memory>
#include <string>

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

    // Optional. A bare state-bound identifier/expression in .avaui
    // source (e.g. `text = errorFile`) parses as opaque
    // PropertyType::String holding its own raw source text ("errorFile")
    // -- the same representation a literal string would use -- and only
    // gets resolved to its actual runtime value later, at render-tree
    // build time (see RenderTree::Eval). Compute()'s intrinsic-size pass
    // runs *before* that, so without an evaluator, a Text/Button/Label/
    // Link/TextBox/ComboBox/CheckBox/RadioButton whose content is
    // state-bound gets measured using its own unresolved source
    // expression rather than what will actually be displayed -- which
    // for anything but a trivially short bound value produces a
    // too-narrow intrinsic size (and everything downstream that sizes
    // off it: auto-sized columns/rows, ellipsis clipping, ...).
    //
    // Set this (mirroring IRenderTree::SetEvalText, same callback shape)
    // before calling Compute() to have text-ish properties resolved
    // through it first. Leaving it unset (the default) preserves the
    // previous behavior of measuring the raw property text as-is, which
    // is still correct for literal (non-bound) content and for callers
    // (e.g. design-time preview) with no state evaluator available.
    virtual void SetTextEvaluator(std::function<std::string(const std::string&)> eval) = 0;

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

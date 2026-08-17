#ifndef AVA_UI_LAYOUT_LAYOUTENGINEIMPL_H
#define AVA_UI_LAYOUT_LAYOUTENGINEIMPL_H

#include <memory>
#include <unordered_map>

#include "layout/LayoutEngine.h"
#include "common/NonCopyable.h"
#include "layout/LayoutNode.h"

namespace avalang {
namespace ui {
namespace layout {

// A component's own content-based size -- what it would be if nothing
// external (parent-offered space, "stretch" alignment) forced it to a
// different size. Fase A (see docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md):
// computed bottom-up, once per Compute() call, before the existing
// top-down placement pass runs. Not part of ILayoutNode -- it's an
// intermediate value the engine consumes internally, never exposed to
// callers (they only ever see the final placed LayoutRect).
struct IntrinsicSize {
    double width = 0.0;
    double height = 0.0;
};

class LayoutEngineImpl final : public LayoutEngine, private common::NonCopyable {
public:
    ILayoutNode* Compute(IComponent* componentRoot, const LayoutRect& available) override;
    void SetTextEvaluator(std::function<std::string(const std::string&)> eval) override {
        textEvaluator_ = std::move(eval);
    }
    ILayoutNode* FindNode(ComponentId id) const override;
    ILayoutNode* Root() const override;

private:
    // See LayoutEngine::SetTextEvaluator. Applied in ComputeIntrinsicSize
    // to every text-ish property it measures, before EstimateTextWidth
    // ever sees it; unset means "use the raw property text as-is",
    // matching pre-evaluator behavior.
    std::string EvalText(const std::string& raw) const {
        return textEvaluator_ ? textEvaluator_(raw) : raw;
    }

    std::function<std::string(const std::string&)> textEvaluator_;

    // Mirrors `component` and its subtree into LayoutNode instances
    // (no geometry yet), registering every node in `nodes_`. Order of
    // LayoutNode::Children() matches IComponent::Children() exactly.
    LayoutNode* BuildTree(IComponent* component, LayoutNode* parent);

    // Bottom-up measure pass (Fase A): recurses into `component`'s
    // children first, then derives this component's own intrinsic
    // size from them (Row sums children's main-axis size and takes
    // the max cross-axis; Column is the transpose; Stack/anything else
    // takes the max on both axes), or from its own text content for
    // Button/Text/Label leaves (see layout/TextMeasure.h). An explicit
    // "width"/"height" property always overrides whatever was just
    // derived, on that axis only, so a parent measuring this
    // component sees its authored size rather than its content size.
    // Caches every result in intrinsic_ (keyed by ComponentId) so
    // PlaceComponent/ArrangeRowOrColumn can look it up without
    // recomputing; called once per Compute(), for the whole subtree,
    // before LayoutNodeRecursive's top-down pass starts.
    IntrinsicSize ComputeIntrinsicSize(IComponent* component);

    // Resolves `component`'s own border box within `slot` (the box the
    // parent offers, still including this component's margin) using
    // `hAlign`/`vAlign` for sizing/positioning on each axis, writes it
    // into `node`, then arranges `component`'s children (Row/Column/
    // Stack, dispatched on TypeName()) inside the resulting content
    // box (own rect shrunk by padding). This is the single recursive
    // step every node in the tree goes through, from the root down.
    void LayoutNodeRecursive(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                              LayoutAlignment hAlign, LayoutAlignment vAlign);

    // Computes and writes `node`'s own LayoutRect (margin-shrunk,
    // explicit-size-or-fill per axis, then aligned within the
    // margin-shrunk box). Returns the same rect for the caller's
    // convenience (e.g. to derive the content box for children).
    LayoutRect PlaceComponent(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                               LayoutAlignment hAlign, LayoutAlignment vAlign);

    // Lays `component`'s children out along one axis (horizontal for
    // a Row, vertical for a Column): explicit-size children keep their
    // main-axis size, the rest split the remaining space evenly (no
    // flex weights in this phase). Cross-axis alignment comes from
    // each child's own "align" property; the main axis always behaves
    // as Stretch, since the slot handed to each child is already sized
    // exactly to its allotment.
    //
    // `allowOverflow` is true for ScrollView/Flex (whose whole purpose
    // is to let content exceed the box and scroll/shrink-wrap past it)
    // and false for plain Row/Column. When false, a child whose own
    // intrinsic/fixed main-axis size exceeds the available space is
    // clamped to that available space -- the previous behavior (let it
    // overflow) made an over-sized row of buttons inside a dialog
    // column visibly spill past the dialog card's right edge instead
    // of fitting within the slot the column handed the row. ScrollView
    // passes true because its scrolling contract depends on children's
    // rects actually extending past contentBox on the scroll axis (see
    // SceneCommandWalker's ScrollView branch, which turns that overflow
    // into a real scrollable region); clamping there would clip the
    // scroll content to the viewport and break scrolling.
    void ArrangeRowOrColumn(IComponent* component, LayoutNode* node, const LayoutRect& contentBox, bool isRow, bool allowOverflow = false);

    // Overlays every child of `component` on the same `contentBox`,
    // each positioned/sized independently via its own "align-h"/
    // "align-v" properties. Used for TypeName() == "Stack" and as the
    // fallback for any TypeName the engine doesn't otherwise recognize.
    void ArrangeStack(IComponent* component, LayoutNode* node, const LayoutRect& contentBox);

    void ArrangeGrid(IComponent* component, LayoutNode* node, const LayoutRect& contentBox);

    std::unordered_map<ComponentId, std::unique_ptr<LayoutNode>> nodes_;
    LayoutNode* root_ = nullptr;

    // The `available` rect Compute() was called with -- i.e. the full
    // page/slot viewport, before any Row/Column/Stack has narrowed it
    // down for a particular descendant. TypeName() == "Dialog" reads
    // this in LayoutNodeRecursive to center itself against the whole
    // page instead of wherever its parent's flow would have placed it
    // (see that function's own comment for why an in-flow Dialog is
    // wrong). Set once per Compute() call, alongside nodes_/intrinsic_.
    LayoutRect rootViewport_;

    // Fase A: intrinsic size of every component in the current
    // Compute() call's subtree, filled by ComputeIntrinsicSize before
    // placement starts, cleared at the top of every Compute().
    std::unordered_map<ComponentId, IntrinsicSize> intrinsic_;
};

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_LAYOUTENGINEIMPL_H

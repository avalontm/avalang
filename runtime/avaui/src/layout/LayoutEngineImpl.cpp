#include "layout/LayoutEngineImpl.h"

#include <algorithm>
#include <vector>

#include "layout/LayoutProperties.h"
#include "layout/TextMeasure.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace layout {

namespace {

// Position of a child's own size within the length its parent offers
// it, per alignment. Start/Stretch both place the child flush at the
// start of the slot -- for Stretch that's moot since childLength ==
// slotLength by construction (see PlaceComponent).
double AlignOffset(double slotStart, double slotLength, double childLength, LayoutAlignment align) {
    switch (align) {
        case LayoutAlignment::Center:
            return slotStart + (slotLength - childLength) / 2.0;
        case LayoutAlignment::End:
            return slotStart + (slotLength - childLength);
        case LayoutAlignment::Start:
        case LayoutAlignment::Stretch:
        default:
            return slotStart;
    }
}

// ScrollView has no dedicated LayoutProperties.h entry (it's a single
// ad-hoc string read, not a family of aliases/edge-insets like
// margin/padding) -- reads "direction" straight off the component,
// same inline style as the "text"/"fontName" reads already in
// ComputeIntrinsicSize below. Anything other than "horizontal" (absent,
// wrong type, "vertical", typo) falls back to vertical, same
// soft-fallback convention as ReadAlignment.
bool IsHorizontalDirection(const IComponent* component) {
    if (const PropertyValue* value = component->GetProperty("direction")) {
        if (value->Type() == PropertyType::String) {
            return value->AsString() == "horizontal";
        }
    }
    return false;
}

// A Dialog is a modal: wherever it sits in the component tree, it
// must be positioned/centered against the whole page, never against
// whatever slot its parent Row/Column/Stack would otherwise hand it
// (see LayoutNodeRecursive's own comment). Checked purely on
// TypeName() -- no state lookup needed (LayoutEngine has no access to
// the state evaluator at all, see docs/architecture/
// property-state-binding.md's "Límite conocido" section), and none is
// needed here: centering math is the same whether the dialog ends up
// open or closed, and RenderTree already skips painting/children
// entirely for a closed one.
bool IsDialog(const IComponent* component) {
    return component->TypeName() == "Dialog";
}

// A Slot marks where a page's own content lands inside a layout (see
// `slot()` in samples/web/testproj/layouts/main.avaui and
// ui_pipeline_dynamic_renderer.cpp's "locate layout slot" probe pass,
// which runs LayoutEngine::Compute against the *layout* tree alone --
// before the calling page's content has been substituted in -- purely
// to read back the Slot's own rect). At that probe time a Slot has no
// children yet, so its intrinsic size (ComputeIntrinsicSize's generic
// Stack-like fallback, since "Slot" isn't Row/Column/ScrollView/a known
// leaf) is always 0x0. Without this special case, a Slot with no
// explicit width/height and no explicit `grow = true` -- which is how
// every real layout writes it; the *wrapping* Row/Column gets
// `grow = true`, not the Slot itself, see main.avaui -- falls into
// ArrangeRowOrColumn's ordinary "hugs its own content" branch and
// claims zero main-axis space, so the page ends up rendered into a
// 0-height (or 0-width) box no matter how much room its parent
// actually has. A Slot is, definitionally, the one child meant to
// consume whatever room its parent has left -- so treat it as an
// implicit `grow = true` unless the author gave it an explicit
// width/height (still respected by ArrangeRowOrColumn's Pass 1 above
// this check runs) or explicitly opted out with `grow = false`.
bool IsSlot(const IComponent* component) {
    return component->TypeName() == "Slot";
}

// Opt-in multi-line wrapping for Text/Label (see TextMeasure.h's
// WrapTextLines). Off by default -- every existing Text/Label in a
// project keeps today's single-line, intrinsic-width-hugs-the-text
// behavior unless the author explicitly asks to wrap. Wrapping only
// actually takes effect when the component ALSO has an explicit
// `width` (see the Text/Label branch below): ComputeIntrinsicSize
// runs bottom-up, before any top-down slot/available-width is known,
// so a wrap target has to come from something already known at this
// point -- an author-given width is that anchor, the same anchor
// `width` already is for every other explicit-size override in this
// function.
bool ReadWrapFlag(const IComponent* component) {
    const PropertyValue* value = component->GetProperty("wrap");
    return value && value->Type() == PropertyType::Bool && value->AsBool();
}

// True if `component` should claim a share of its Row/Column parent's
// leftover main-axis space: either an explicit `grow = true`, or a
// Slot that hasn't explicitly opted out (see IsSlot above). Checked
// once, from ArrangeRowOrColumn's Pass 1, so a Slot's implicit default
// and an author's explicit override share one place instead of two
// separate opinions about the same child.
bool ShouldGrow(const IComponent* component) {
    const PropertyValue* growProp = component->GetProperty("grow");
    if (growProp && growProp->Type() == PropertyType::Bool) {
        return growProp->AsBool();
    }
    return IsSlot(component);
}

} // namespace

ILayoutNode* LayoutEngineImpl::Compute(IComponent* componentRoot, const LayoutRect& available) {
    nodes_.clear();
    intrinsic_.clear();
    root_ = nullptr;
    rootViewport_ = available;

    if (!componentRoot) {
        return nullptr;
    }

    root_ = BuildTree(componentRoot, nullptr);

    // Fase A -- bottom-up measure pass: fills intrinsic_ for every
    // component in the subtree before any top-down placement happens,
    // so PlaceComponent/ArrangeRowOrColumn below can look up a
    // component's own content-based size instead of assuming 0.
    ComputeIntrinsicSize(componentRoot);

    // The root always stretches to fill whatever space the caller
    // offers -- there is no sibling/parent context to align it
    // against.
    LayoutNodeRecursive(componentRoot, root_, available, LayoutAlignment::Stretch, LayoutAlignment::Stretch);

    return root_;
}

IntrinsicSize LayoutEngineImpl::ComputeIntrinsicSize(IComponent* component) {
    IntrinsicSize size;
    if (!component) {
        return size;
    }

    // Children first (post-order) -- a Row/Column/Stack needs its
    // children's intrinsic size to derive its own.
    std::vector<IComponent*> children = component->Children();
    std::vector<IntrinsicSize> childSizes;
    childSizes.reserve(children.size());
    for (IComponent* child : children) {
        childSizes.push_back(ComputeIntrinsicSize(child));
    }

    const std::string& typeName = component->TypeName();
    EdgeInsets padding = ReadEdgeInsets(component, "padding");
    double spacing = ReadNumber(component, "spacing", 0.0);

    if (typeName == "Button" || typeName == "Text" || typeName == "Label" || typeName == "Link") {
        // Leaf content: measure the label text against the component's
        // actual font (real glyph metrics via FontRegistry -- see
        // TextMeasure.h/FontRegistry.h). `fontName` also drives the
        // line-height lookup below so a control that sets a custom
        // font gets that font's real ascent/descent, not the default
        // font's.
        std::string text;
        if (const PropertyValue* value = component->GetProperty("text")) {
            if (value->Type() == PropertyType::String) {
                text = value->AsString();
            }
        }
        double fontSize = ReadNumber(component, "fontSize", kDefaultFontSizePx);
        std::string fontName;
        if (const PropertyValue* value = component->GetProperty("fontName")) {
            if (value->Type() == PropertyType::String) {
                fontName = value->AsString();
            }
        }
        double textWidth = EstimateTextWidth(text, fontSize, fontName);
        double textHeight = DefaultLineHeight(fontSize, fontName);

        if (typeName == "Button") {
            // A button additionally gets default padding around its
            // label and a minimum height, so a one-character label
            // (e.g. "-") still gets a reasonable, clickable size.
            size.width = textWidth + 2.0 * kDefaultButtonPaddingX;
            size.height = std::max(kDefaultButtonMinHeight, textHeight + 2.0 * kDefaultButtonPaddingY);
        } else {
            size.width = textWidth;
            size.height = textHeight;

            // Wrap (Text/Label only, not Button/Link -- see
            // ReadWrapFlag): needs an explicit `width` to wrap against,
            // read directly here rather than waiting for the generic
            // explicit-width override below, since that override runs
            // after this branch and this is the one place that also
            // needs to know the *wrapped* height, not just the width.
            // A component with `wrap = true` but no explicit width
            // falls through unchanged (single line, hugs textWidth) --
            // there's nothing to wrap against yet at this bottom-up
            // pass (see ReadWrapFlag's comment).
            double explicitWidthForWrap = 0.0;
            if (ReadWrapFlag(component) &&
                TryReadNumber(component, "width", &explicitWidthForWrap) &&
                explicitWidthForWrap > 0.0 &&
                textWidth > explicitWidthForWrap) {
                std::vector<std::string> lines =
                    WrapTextLines(text, fontSize, fontName, explicitWidthForWrap);
                double lineHeight = WrappedLineHeight(fontSize, fontName);
                size.width = explicitWidthForWrap;
                size.height = lineHeight * static_cast<double>(lines.size());
            }
        }
    } else if (typeName == "TextBox" || typeName == "ComboBox") {
        // Neither has label text to measure the way Button/Text do
        // (TextBox's "text" is editable content, ComboBox's options
        // live on its children) -- fall back to a fixed form-field
        // floor, same shape as Button's min-height/padding pattern.
        std::string content;
        if (const PropertyValue* value = component->GetProperty("text")) {
            if (value->Type() == PropertyType::String) {
                content = value->AsString();
            }
        }
        if (content.empty()) {
            if (const PropertyValue* placeholder = component->GetProperty("placeholder")) {
                if (placeholder->Type() == PropertyType::String) {
                    content = placeholder->AsString();
                }
            }
        }
        double fontSize = ReadNumber(component, "fontSize", kDefaultFontSizePx);
        double textWidth = EstimateTextWidth(content, fontSize, std::string());
        double textHeight = DefaultLineHeight(fontSize);
        size.width = std::max(kDefaultInputMinWidth, textWidth + 2.0 * kDefaultInputPaddingX);
        size.height = std::max(kDefaultInputMinHeight, textHeight + 2.0 * (kDefaultInputPaddingX * 0.5));
    } else if (typeName == "CheckBox" || typeName == "RadioButton") {
        // Fixed box (see RenderTree::DecomposeCheckBox/
        // DecomposeRadioButton, which caps the same box at 16px) plus
        // the label text next to it.
        std::string label;
        if (const PropertyValue* value = component->GetProperty("label")) {
            if (value->Type() == PropertyType::String) {
                label = value->AsString();
            }
        }
        double fontSize = ReadNumber(component, "fontSize", kDefaultFontSizePx);
        double labelWidth = label.empty() ? 0.0 : EstimateTextWidth(label, fontSize, std::string());
        double labelGap = label.empty() ? 0.0 : kDefaultCheckboxLabelGap;
        double textHeight = DefaultLineHeight(fontSize);
        size.width = kDefaultCheckboxBoxSize + labelGap + labelWidth;
        size.height = std::max(kDefaultCheckboxBoxSize, textHeight);
    } else if (typeName == "Icon") {
        size.width = kDefaultIconSize;
        size.height = kDefaultIconSize;
    } else if (typeName == "Image") {
        size.width = kDefaultImageSize;
        size.height = kDefaultImageSize;
    } else if (typeName == "Row") {
        double mainTotal = 0.0;
        double crossMax = 0.0;
        for (const IntrinsicSize& childSize : childSizes) {
            mainTotal += childSize.width;
            crossMax = std::max(crossMax, childSize.height);
        }
        if (!childSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childSizes.size() - 1);
        }
        size.width = mainTotal + padding.left + padding.right;
        size.height = crossMax + padding.top + padding.bottom;
    } else if (typeName == "Column" || typeName == "For" || typeName == "If") {
        double mainTotal = 0.0;
        double crossMax = 0.0;
        for (const IntrinsicSize& childSize : childSizes) {
            mainTotal += childSize.height;
            crossMax = std::max(crossMax, childSize.width);
        }
        if (!childSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childSizes.size() - 1);
        }
        size.height = mainTotal + padding.top + padding.bottom;
        size.width = crossMax + padding.left + padding.right;
    } else if (typeName == "ScrollView" || typeName == "Flex") {
        // Same Row/Column math (dispatched on "direction"), for the case
        // a ScrollView has no explicit width/height of its own -- without
        // this, an un-sized ScrollView would intrinsic-size to 0x0 (the
        // generic Stack-like fallback below, which just takes the max
        // child on each axis, not the sum along the scroll axis) and
        // clip every child to nothing. In practice most ScrollViews get
        // an explicit height (that's the point -- a bounded viewport
        // smaller than its content), so this mostly matters for the
        // cross axis and for nested/auto-sizing edge cases.
        bool isRow = IsHorizontalDirection(component);
        double mainTotal = 0.0;
        double crossMax = 0.0;
        for (const IntrinsicSize& childSize : childSizes) {
            mainTotal += isRow ? childSize.width : childSize.height;
            crossMax = std::max(crossMax, isRow ? childSize.height : childSize.width);
        }
        if (!childSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childSizes.size() - 1);
        }
        if (isRow) {
            size.width = mainTotal + padding.left + padding.right;
            size.height = crossMax + padding.top + padding.bottom;
        } else {
            size.height = mainTotal + padding.top + padding.bottom;
            size.width = crossMax + padding.left + padding.right;
        }
    } else if (typeName == "Grid") {
        int columns = std::max(1, static_cast<int>(ReadNumber(component, "columns", 1.0)));
        int neededRows = childSizes.empty()
                              ? 0
                              : static_cast<int>((childSizes.size() + columns - 1) / static_cast<size_t>(columns));
        int explicitRows = static_cast<int>(ReadNumber(component, "rows", 0.0));
        int rows = std::max({1, neededRows, explicitRows});

        double maxChildWidth = 0.0;
        double maxChildHeight = 0.0;
        for (const IntrinsicSize& childSize : childSizes) {
            maxChildWidth = std::max(maxChildWidth, childSize.width);
            maxChildHeight = std::max(maxChildHeight, childSize.height);
        }

        double totalWidth = maxChildWidth * columns + spacing * (columns - 1);
        double totalHeight = maxChildHeight * rows + spacing * (rows - 1);
        size.width = totalWidth + padding.left + padding.right;
        size.height = totalHeight + padding.top + padding.bottom;
    } else {
        // "Stack" and every TypeName the engine doesn't otherwise
        // recognize -- same overlay scope as ArrangeStack: intrinsic
        // size is just the largest child on each axis, no attempt at
        // a real box model beyond that.
        double maxWidth = 0.0;
        double maxHeight = 0.0;
        for (const IntrinsicSize& childSize : childSizes) {
            maxWidth = std::max(maxWidth, childSize.width);
            maxHeight = std::max(maxHeight, childSize.height);
        }
        size.width = maxWidth + padding.left + padding.right;
        size.height = maxHeight + padding.top + padding.bottom;
    }

    // An explicit width/height always wins over whatever content
    // derived above, on that axis only -- this is what lets intrinsic
    // sizing compose correctly up the tree: a parent measuring this
    // component sees its authored size, not its content size, when
    // the author overrode one or both axes.
    double explicitValue;
    if (TryReadNumber(component, "width", &explicitValue)) {
        size.width = explicitValue;
    }
    if (TryReadNumber(component, "height", &explicitValue)) {
        size.height = explicitValue;
    }

    intrinsic_[component->Id()] = size;
    return size;
}

ILayoutNode* LayoutEngineImpl::FindNode(ComponentId id) const {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : it->second.get();
}

ILayoutNode* LayoutEngineImpl::Root() const {
    return root_;
}

LayoutNode* LayoutEngineImpl::BuildTree(IComponent* component, LayoutNode* parent) {
    auto owned = std::make_unique<LayoutNode>(component->Id());
    LayoutNode* node = owned.get();

    node->SetParent(parent);
    if (parent) {
        parent->AddChild(node);
    }

    nodes_.emplace(component->Id(), std::move(owned));

    for (IComponent* child : component->Children()) {
        BuildTree(child, node);
    }

    return node;
}

LayoutRect LayoutEngineImpl::PlaceComponent(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                                             LayoutAlignment hAlign, LayoutAlignment vAlign) {
    EdgeInsets margin = ReadEdgeInsets(component, "margin");

    LayoutRect marginedBox;
    marginedBox.x = slot.x + margin.left;
    marginedBox.y = slot.y + margin.top;
    marginedBox.width = std::max(0.0, slot.width - margin.left - margin.right);
    marginedBox.height = std::max(0.0, slot.height - margin.top - margin.bottom);

    // Fase A: fallback for a non-stretch axis with no explicit size --
    // previously always 0.0 (documented Phase 3 limitation), now this
    // component's own intrinsic/content-based size, computed by
    // ComputeIntrinsicSize before this pass started (see Compute()).
    // Falls back to a zero IntrinsicSize (same as the old behavior) if
    // this component somehow isn't in the cache.
    auto intrinsicIt = intrinsic_.find(component->Id());
    const IntrinsicSize intrinsicSize = intrinsicIt != intrinsic_.end() ? intrinsicIt->second : IntrinsicSize{};

    // A Dialog is exempted from the normal "clamp to available space"
    // rule below. It's centered against rootViewport_ (LayoutNodeRecursive)
    // purely so it can float freely over the whole page -- that slot was
    // never meant to also cap its SIZE. Row/Column/Stack arrangement
    // (ArrangeRowOrColumn/ArrangeStack) never shrinks a child to fit
    // whatever box its parent ended up with -- every child keeps its own
    // intrinsic/explicit size and is simply stacked -- so clamping the
    // Dialog's own height to marginedBox.height while its inner column
    // (title + message + button row) kept its full natural height meant
    // the last child (typically the button row) got positioned past the
    // bottom edge of the now-too-short card, visibly spilling outside the
    // dialog's own painted background. Always sizing a Dialog to its real
    // intrinsic content instead keeps the card and its content consistent;
    // on a viewport too short for it, the worst case is the centered card
    // extending past the top/bottom edge (clipped by #ava-viewport's own
    // overflow:hidden) rather than its buttons detaching from the card.
    const bool isDialog = IsDialog(component);

    double explicitWidth;
    double width;
    if (TryReadNumber(component, "width", &explicitWidth)) {
        width = isDialog ? explicitWidth : std::min(explicitWidth, marginedBox.width);
    } else if (hAlign == LayoutAlignment::Stretch) {
        width = marginedBox.width;
    } else {
        width = isDialog ? intrinsicSize.width : std::min(intrinsicSize.width, marginedBox.width);
    }

    double explicitHeight;
    double height;
    if (TryReadNumber(component, "height", &explicitHeight)) {
        height = isDialog ? explicitHeight : std::min(explicitHeight, marginedBox.height);
    } else if (vAlign == LayoutAlignment::Stretch) {
        height = marginedBox.height;
    } else {
        height = isDialog ? intrinsicSize.height : std::min(intrinsicSize.height, marginedBox.height);
    }

    LayoutRect ownRect;
    ownRect.x = AlignOffset(marginedBox.x, marginedBox.width, width, hAlign);
    ownRect.y = AlignOffset(marginedBox.y, marginedBox.height, height, vAlign);
    ownRect.width = width;
    ownRect.height = height;

    node->SetRect(ownRect);
    return ownRect;
}

void LayoutEngineImpl::LayoutNodeRecursive(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                                            LayoutAlignment hAlign, LayoutAlignment vAlign) {
    // A Dialog ignores the slot/alignment its parent's flow computed
    // for it -- it isn't part of that flow visually (RenderTree/
    // SceneCommandWalker paint it as a top-level overlay, see
    // DecomposeDialog's own comment), so laying it out as if it were
    // just another Row/Column child put its box wherever that flow
    // happened to place it: often far down the page, in practice
    // exactly the bug this fixes (an open Dialog with a visible
    // backdrop but no visible box, because the box's rect landed
    // outside the visible/clipped area). Center it against the page's
    // own root viewport instead, sized to its own content
    // (intrinsic size, or an explicit width/height property) same as
    // any other component -- PlaceComponent below still does that
    // part unchanged, only the box it centers within changes.
    const LayoutRect& effectiveSlot = IsDialog(component) ? rootViewport_ : slot;
    const LayoutAlignment effectiveHAlign = IsDialog(component) ? LayoutAlignment::Center : hAlign;
    const LayoutAlignment effectiveVAlign = IsDialog(component) ? LayoutAlignment::Center : vAlign;

    LayoutRect ownRect = PlaceComponent(component, node, effectiveSlot, effectiveHAlign, effectiveVAlign);

    EdgeInsets padding = ReadEdgeInsets(component, "padding");
    LayoutRect contentBox;
    contentBox.x = ownRect.x + padding.left;
    contentBox.y = ownRect.y + padding.top;
    contentBox.width = std::max(0.0, ownRect.width - padding.left - padding.right);
    contentBox.height = std::max(0.0, ownRect.height - padding.top - padding.bottom);

    const std::string& typeName = component->TypeName();
    if (typeName == "Row") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/true, /*allowOverflow=*/false);
    } else if (typeName == "Column" || typeName == "For" || typeName == "If") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/false, /*allowOverflow=*/false);
    } else if (typeName == "ScrollView") {
        // Structurally identical to Row/Column -- a ScrollView is just a
        // Row/Column whose content is allowed to be taller/wider than the
        // box itself. ArrangeRowOrColumn already doesn't clamp a fixed-
        // or intrinsic-sized child to the available main-axis space (see
        // its own comment on `sizingPool`/`remaining`), so children of a
        // ScrollView shorter/narrower than their content already end up
        // with Rects that extend past `contentBox` on the scroll axis --
        // exactly the overflow SceneCommandWalker turns into a real
        // scrollable region on web (see its "ScrollView" case).
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/IsHorizontalDirection(component), /*allowOverflow=*/true);
    } else if (typeName == "Flex") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/IsHorizontalDirection(component), /*allowOverflow=*/true);
    } else if (typeName == "Grid") {
        ArrangeGrid(component, node, contentBox);
    } else {
        // "Stack" and every TypeName the engine doesn't otherwise
        // recognize (Page, Container, Text, Button, ...) share the
        // same overlay behavior -- see LayoutEngine.h and
        // docs/AVALANG_UI_IMPLEMENTATION_PLAN.md, "Component Model".
        // Control-specific arrangement is a later phase (controls/);
        // Phase 3 only needs a sane default for "a container with
        // children".
        ArrangeStack(component, node, contentBox);
    }
}

void LayoutEngineImpl::ArrangeRowOrColumn(IComponent* component, LayoutNode* node, const LayoutRect& contentBox,
                                           bool isRow, bool allowOverflow) {
    std::vector<IComponent*> children = component->Children();
    if (children.empty()) {
        return;
    }

    const std::vector<ILayoutNode*>& childNodes = node->Children();

    double spacing = ReadNumber(component, "spacing", 0.0);
    double mainAvailable = (isRow ? contentBox.width : contentBox.height)
                            - spacing * static_cast<double>(children.size() - 1);
    mainAvailable = std::max(0.0, mainAvailable);

    // Pass 1: reserve each child's margin on the main axis, and
    // resolve each child's main-axis content size ("width" for a Row,
    // "height" for a Column) from, in order:
    //
    //   1. An explicit `width`/`height` property -- always wins,
    //      whatever kind of child this is.
    //   2. An explicit `grow = true` (or a Slot, which grows
    //      implicitly unless it opts out with `grow = false` -- see
    //      IsSlot/ShouldGrow above) -- this child claims a share of
    //      whatever main-axis space its fixed/intrinsic siblings don't
    //      use (e.g. a content column sandwiched between a Navbar and
    //      a Footer that both size to their own content).
    //   3. Any other child -- a nested layout container
    //      (Row/Column/Stack/Container/Page/ScrollView) OR a leaf/
    //      atomic control (Button, Text, TextBox, CheckBox, Image,
    //      ...) -- with no explicit size and no `grow = true` hugs its
    //      own intrinsic/content-based size (Fase A). This is what
    //      makes a typical `[logo, row(grow=true), row{link, link}]`
    //      navbar behave as authors expect: the logo and the link
    //      group each take only as much width as their own content
    //      needs, and only the explicitly-`grow`-marked spacer
    //      between them claims whatever space is left over. Without
    //      this fallback, every non-explicit child -- including ones
    //      that never asked to grow -- competed equally for the
    //      remaining space, so e.g. two links meant to sit tight next
    //      to each other on the right instead got stretched across
    //      half the navbar each.
    const char* mainProp = isRow ? "width" : "height";
    std::vector<double> childMarginMain(children.size(), 0.0);
    std::vector<double> childMainSize(children.size(), -1.0);
    double totalMarginMain = 0.0;
    double fixedTotal = 0.0;
    int autoCount = 0;

    for (size_t i = 0; i < children.size(); ++i) {
        if (IsDialog(children[i])) {
            // Positioned/centered against the root viewport by
            // LayoutNodeRecursive, not this Row/Column's flow --
            // reserves zero main-axis space and zero margin here so
            // its siblings lay out exactly as if it weren't present
            // (an open modal shouldn't leave a gap where it "would
            // have" sat in the list).
            childMainSize[i] = 0.0;
            continue;
        }

        EdgeInsets childMargin = ReadEdgeInsets(children[i], "margin");
        childMarginMain[i] = isRow ? (childMargin.left + childMargin.right)
                                    : (childMargin.top + childMargin.bottom);
        totalMarginMain += childMarginMain[i];

        double explicitMain;
        if (TryReadNumber(children[i], mainProp, &explicitMain)) {
            childMainSize[i] = explicitMain;
            fixedTotal += explicitMain;
            continue;
        }

        if (ShouldGrow(children[i])) {
            ++autoCount;
            continue;
        }

        // Any other child -- container or leaf -- with no explicit
        // size and no `grow = true` hugs its own content: its
        // main-axis footprint is its intrinsic size (Fase A,
        // ComputeIntrinsicSize, already computed for every component
        // before this pass runs -- see Compute()), same source
        // PlaceComponent already falls back to for a non-Stretch
        // cross axis. Treated as fixed (added to fixedTotal, not
        // autoCount) so it does NOT compete with `grow = true`
        // siblings for the remaining space -- only an explicit `grow`
        // does that. Falls back to 0 if this component somehow isn't
        // in the cache (matches PlaceComponent's own fallback).
        auto intrinsicIt = intrinsic_.find(children[i]->Id());
        double intrinsicMain = intrinsicIt != intrinsic_.end()
                                    ? (isRow ? intrinsicIt->second.width : intrinsicIt->second.height)
                                    : 0.0;
        childMainSize[i] = intrinsicMain;
        fixedTotal += intrinsicMain;
    }

    double sizingPool = std::max(0.0, mainAvailable - totalMarginMain);
    double remaining = std::max(0.0, sizingPool - fixedTotal);
    double autoShare = autoCount > 0 ? remaining / autoCount : 0.0;

    // When `allowOverflow == false` (plain Row/Column, not ScrollView/
    // Flex) and the fixed/intrinsic children together exceed the
    // available main-axis space, scale every fixed child's main-axis
    // size down proportionally so they collectively fit. This is the
    // fix for the over-sized dialog button row spilling past the dialog
    // card's right edge: a Row of two buttons with no explicit `width`
    // hugs its intrinsic content (~363px of button text + padding + a
    // 12px gap), which is wider than the 320px slot the dialog's inner
    // Column handed it, and without this proportional shrink the row
    // simply overflowed to the right (Aceptar's right edge 35px past
    // the dialog card). ScrollView/Flex pass `allowOverflow == true`
    // because their whole purpose is to let content exceed the box and
    // scroll/shrink-wrap past it -- shrinking there would clip the
    // scrollable content to the viewport and break scrolling. Only
    // fixed children are scaled (childMainSize[i] >= 0.0): grow
    // children already took `autoShare`, which is 0 when there's no
    // remaining space (so they collapse to zero width rather than
    // contribute to the overflow), and Dialogs are exempt (positioned/
    // centered against rootViewport_, must keep their full size).
    // Per-child clamping (the earlier attempt) was wrong: clamping each
    // child to the FULL mainAvailable left every child claiming the
    // entire pool, so two buttons each took 308px and still overflowed.
    // Scaling proportionally divides the pool among all fixed children
    // so their sum fits.
    double fixedScale = 1.0;
    if (!allowOverflow && fixedTotal > sizingPool && fixedTotal > 0.0) {
        fixedScale = sizingPool / fixedTotal;
    }

    // Pass 2: place each child in turn along the main axis. Cross-axis
    // alignment is the child's own "align" property; the main axis is
    // always Stretch, since the slot we hand each child is already sized
    // to exactly its allotment (fixed/intrinsic size scaled by
    // fixedScale, or autoShare).
    double cursor = isRow ? contentBox.x : contentBox.y;
    for (size_t i = 0; i < children.size(); ++i) {
        double mainContentSize = childMainSize[i] >= 0.0 ? childMainSize[i] * fixedScale : autoShare;
        double slotMainLength = mainContentSize + childMarginMain[i];

        LayoutRect childSlot;
        if (isRow) {
            childSlot.x = cursor;
            childSlot.y = contentBox.y;
            childSlot.width = slotMainLength;
            childSlot.height = contentBox.height;
        } else {
            childSlot.x = contentBox.x;
            childSlot.y = cursor;
            childSlot.width = contentBox.width;
            childSlot.height = slotMainLength;
        }

        LayoutAlignment crossAlign = ReadAlignment(children[i], "align");
        LayoutAlignment hAlign = isRow ? LayoutAlignment::Stretch : crossAlign;
        LayoutAlignment vAlign = isRow ? crossAlign : LayoutAlignment::Stretch;

        LayoutNodeRecursive(children[i], static_cast<LayoutNode*>(childNodes[i]), childSlot, hAlign, vAlign);

        if (!IsDialog(children[i])) {
            cursor += slotMainLength + spacing;
        }
    }
}

void LayoutEngineImpl::ArrangeStack(IComponent* component, LayoutNode* node, const LayoutRect& contentBox) {
    std::vector<IComponent*> children = component->Children();
    const std::vector<ILayoutNode*>& childNodes = node->Children();

    for (size_t i = 0; i < children.size(); ++i) {
        LayoutAlignment hAlign = ReadAlignment(children[i], "align-h");
        LayoutAlignment vAlign = ReadAlignment(children[i], "align-v");
        LayoutNodeRecursive(children[i], static_cast<LayoutNode*>(childNodes[i]), contentBox, hAlign, vAlign);
    }
}

void LayoutEngineImpl::ArrangeGrid(IComponent* component, LayoutNode* node, const LayoutRect& contentBox) {
    std::vector<IComponent*> children = component->Children();
    if (children.empty()) {
        return;
    }

    const std::vector<ILayoutNode*>& childNodes = node->Children();

    int columns = std::max(1, static_cast<int>(ReadNumber(component, "columns", 1.0)));
    int neededRows = static_cast<int>((children.size() + static_cast<size_t>(columns) - 1)
                                       / static_cast<size_t>(columns));
    int explicitRows = static_cast<int>(ReadNumber(component, "rows", 0.0));
    int rows = std::max({1, neededRows, explicitRows});

    double spacing = ReadNumber(component, "spacing", 0.0);
    double cellWidth = std::max(0.0, (contentBox.width - spacing * (columns - 1)) / columns);
    double cellHeight = std::max(0.0, (contentBox.height - spacing * (rows - 1)) / rows);

    for (size_t i = 0; i < children.size(); ++i) {
        int col = static_cast<int>(i) % columns;
        int row = static_cast<int>(i) / columns;

        LayoutRect cellSlot;
        cellSlot.x = contentBox.x + col * (cellWidth + spacing);
        cellSlot.y = contentBox.y + row * (cellHeight + spacing);
        cellSlot.width = cellWidth;
        cellSlot.height = cellHeight;

        LayoutAlignment hAlign = ReadAlignment(children[i], "align-h");
        LayoutAlignment vAlign = ReadAlignment(children[i], "align-v");
        LayoutNodeRecursive(children[i], static_cast<LayoutNode*>(childNodes[i]), cellSlot, hAlign, vAlign);
    }
}

} // namespace layout
} // namespace ui
} // namespace avalang
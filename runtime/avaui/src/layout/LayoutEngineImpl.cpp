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
bool IsHorizontalScrollView(const IComponent* component) {
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
        // Leaf content: measure the label text (see TextMeasure.h for
        // why this is a heuristic, not a pixel-exact glyph measure).
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
        double textHeight = DefaultLineHeight(fontSize);

        if (typeName == "Button") {
            // A button additionally gets default padding around its
            // label and a minimum height, so a one-character label
            // (e.g. "-") still gets a reasonable, clickable size.
            size.width = textWidth + 2.0 * kDefaultButtonPaddingX;
            size.height = std::max(kDefaultButtonMinHeight, textHeight + 2.0 * kDefaultButtonPaddingY);
        } else {
            size.width = textWidth;
            size.height = textHeight;
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
    } else if (typeName == "Column") {
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
    } else if (typeName == "ScrollView") {
        // Same Row/Column math (dispatched on "direction"), for the case
        // a ScrollView has no explicit width/height of its own -- without
        // this, an un-sized ScrollView would intrinsic-size to 0x0 (the
        // generic Stack-like fallback below, which just takes the max
        // child on each axis, not the sum along the scroll axis) and
        // clip every child to nothing. In practice most ScrollViews get
        // an explicit height (that's the point -- a bounded viewport
        // smaller than its content), so this mostly matters for the
        // cross axis and for nested/auto-sizing edge cases.
        bool isRow = IsHorizontalScrollView(component);
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

    double explicitWidth;
    double width;
    if (TryReadNumber(component, "width", &explicitWidth)) {
        width = std::min(explicitWidth, marginedBox.width);
    } else if (hAlign == LayoutAlignment::Stretch) {
        width = marginedBox.width;
    } else {
        width = std::min(intrinsicSize.width, marginedBox.width);
    }

    double explicitHeight;
    double height;
    if (TryReadNumber(component, "height", &explicitHeight)) {
        height = std::min(explicitHeight, marginedBox.height);
    } else if (vAlign == LayoutAlignment::Stretch) {
        height = marginedBox.height;
    } else {
        height = std::min(intrinsicSize.height, marginedBox.height);
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
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/true);
    } else if (typeName == "Column") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/false);
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
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/IsHorizontalScrollView(component));
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
                                           bool isRow) {
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
    //   2. An explicit `grow = true` -- this child claims a share of
    //      whatever main-axis space its fixed/intrinsic siblings don't
    //      use (e.g. a content column sandwiched between a Navbar and
    //      a Footer that both size to their own content). `grow`
    //      deliberately overrides intrinsic sizing rather than only
    //      applying when intrinsic is zero: a padding-only container
    //      (e.g. a Column wrapping a single `slot()`, whose own
    //      intrinsic size is just its padding) would otherwise be
    //      misclassified as "fixed" at its minimal padding size and
    //      never receive the remaining space.
    //   3. Any other child -- a nested layout container
    //      (Row/Column/Stack/Container/Page/ScrollView) OR a leaf/
    //      atomic control (Button, Text, TextBox, CheckBox, Image,
    //      ...) -- with no explicit size defaults to "auto" too, same
    //      as an explicit `grow = true`, WITHOUT needing one: one such
    //      child fills 100% of the main axis, two split it 50/50,
    //      three 33/33/33, and so on -- same rule as an author-facing
    //      Figma-style "fill" default / XAML star-sizing ("*"). A
    //      child's own intrinsic/content-based footprint (Fase A) is
    //      deliberately NOT used as a fallback here for either
    //      containers or leaves: the only way to opt a child out of
    //      the equal-share pool is an explicit `width`/`height`
    //      (step 1) -- there is no more "shrink to fit my label"
    //      default, so the container-vs-leaf distinction this
    //      function used to make no longer matters here.
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

        const PropertyValue* growProp = children[i]->GetProperty("grow");
        if (growProp && growProp->Type() == PropertyType::Bool && growProp->AsBool()) {
            ++autoCount;
            continue;
        }

        // Step 3 (see the Pass 1 comment above): any child -- container
        // or leaf -- with no explicit size and no `grow = true` joins
        // the equal-share pool. No intrinsic/content-based fallback
        // here anymore; that's still used elsewhere (e.g.
        // PlaceComponent's cross-axis sizing), just not as a main-axis
        // default in Row/Column.
        ++autoCount;
    }

    double sizingPool = std::max(0.0, mainAvailable - totalMarginMain);
    double remaining = std::max(0.0, sizingPool - fixedTotal);
    double autoShare = autoCount > 0 ? remaining / autoCount : 0.0;

    // Pass 2: place each child in turn along the main axis. Cross-axis
    // alignment is the child's own "align" property; the main axis is
    // always Stretch, since the slot we hand each child is already
    // sized to exactly its allotment (fixed/intrinsic size or
    // autoShare).
    double cursor = isRow ? contentBox.x : contentBox.y;
    for (size_t i = 0; i < children.size(); ++i) {
        double mainContentSize = childMainSize[i] >= 0.0 ? childMainSize[i] : autoShare;
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

} // namespace layout
} // namespace ui
} // namespace avalang
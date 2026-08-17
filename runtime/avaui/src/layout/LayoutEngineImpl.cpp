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

bool IsHorizontalDirection(const IComponent* component) {
    if (const PropertyValue* value = component->GetProperty("direction")) {
        if (value->Type() == PropertyType::String) {
            return value->AsString() == "horizontal";
        }
    }
    return false;
}

bool IsDialog(const IComponent* component) {
    return component->TypeName() == "Dialog";
}

bool IsSlot(const IComponent* component) {
    return component->TypeName() == "Slot";
}

// A grid cell's default alignment depends on whether the item opts out of
// stretching via an explicit width/height. No explicit size -> fill the
// cell (Stretch), matching every other container. An explicit size means
// the item won't fill the cell either way, so it defaults to Center
// instead of Stretch (which visually behaves like Start, pinning the item
// to the cell's top-left corner). An explicit align-h/align-v always wins.
LayoutAlignment ReadGridChildAlignment(const IComponent* component, const std::string& alignProp,
                                        const std::string& sizeProp) {
    const PropertyValue* alignValue = component->GetProperty(alignProp);
    if (alignValue && alignValue->Type() == PropertyType::String) {
        return ReadAlignment(component, alignProp);
    }
    double unusedSize;
    if (TryReadNumber(component, sizeProp, &unusedSize)) {
        return LayoutAlignment::Center;
    }
    return LayoutAlignment::Stretch;
}

bool ReadWrapFlag(const IComponent* component) {
    const PropertyValue* value = component->GetProperty("wrap");
    return value && value->Type() == PropertyType::Bool && value->AsBool();
}

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

    ComputeIntrinsicSize(componentRoot);

    LayoutNodeRecursive(componentRoot, root_, available, LayoutAlignment::Stretch, LayoutAlignment::Stretch);

    return root_;
}

IntrinsicSize LayoutEngineImpl::ComputeIntrinsicSize(IComponent* component) {
    IntrinsicSize size;
    if (!component) {
        return size;
    }

    std::vector<IComponent*> children = component->Children();
    // childSizes holds each child's own content size (no margin) --
    // this is what gets cached in intrinsic_[] and is what
    // ArrangeRowOrColumn/PlaceComponent read later, adding the
    // child's margin back in separately when carving up the actual
    // slot. childBoxSizes is the margin BOX size (content + margin)
    // and is what a *parent* must use when it aggregates children to
    // size itself -- otherwise the parent bubbles up a size that has
    // no room for its children's margins, and by the time Arrange
    // reserves that margin space at render time there's nothing left
    // to reserve it from (the child's explicit size gets clamped down
    // to fit the leftover sliver). See CartItem's circular remove
    // button losing its height to exactly this.
    std::vector<IntrinsicSize> childSizes;
    std::vector<IntrinsicSize> childBoxSizes;
    childSizes.reserve(children.size());
    childBoxSizes.reserve(children.size());
    for (IComponent* child : children) {
        IntrinsicSize childSize = ComputeIntrinsicSize(child);
        EdgeInsets childMargin = ReadEdgeInsets(child, "margin");
        IntrinsicSize childBoxSize;
        childBoxSize.width = childSize.width + childMargin.left + childMargin.right;
        childBoxSize.height = childSize.height + childMargin.top + childMargin.bottom;
        childSizes.push_back(childSize);
        childBoxSizes.push_back(childBoxSize);
    }

    const std::string& typeName = component->TypeName();
    EdgeInsets padding = ReadEdgeInsets(component, "padding");
    double spacing = ReadNumber(component, "spacing", 0.0);

    if (typeName == "Button" || typeName == "Text" || typeName == "Label" || typeName == "Link") {
        std::string text;
        if (const PropertyValue* value = component->GetProperty("text")) {
            if (value->Type() == PropertyType::String) {
                // `text` may be a literal or a bare state-bound
                // identifier/expression -- both parse to the same
                // PropertyType::String (see LayoutEngine::SetTextEvaluator);
                // EvalText resolves the latter to its runtime value so
                // this measures what will actually be displayed instead
                // of the identifier's own source text.
                text = EvalText(value->AsString());
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
            size.width = textWidth + 2.0 * kDefaultButtonPaddingX;
            size.height = std::max(kDefaultButtonMinHeight, textHeight + 2.0 * kDefaultButtonPaddingY);
        } else {
            size.width = textWidth;
            size.height = textHeight;
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
        std::string content;
        if (const PropertyValue* value = component->GetProperty("text")) {
            if (value->Type() == PropertyType::String) {
                content = EvalText(value->AsString());
            }
        }
        if (content.empty()) {
            if (const PropertyValue* placeholder = component->GetProperty("placeholder")) {
                if (placeholder->Type() == PropertyType::String) {
                    content = EvalText(placeholder->AsString());
                }
            }
        }
        double fontSize = ReadNumber(component, "fontSize", kDefaultFontSizePx);
        double textWidth = EstimateTextWidth(content, fontSize, std::string());
        double textHeight = DefaultLineHeight(fontSize);
        size.width = std::max(kDefaultInputMinWidth, textWidth + 2.0 * kDefaultInputPaddingX);
        size.height = std::max(kDefaultInputMinHeight, textHeight + 2.0 * (kDefaultInputPaddingX * 0.5));
    } else if (typeName == "CheckBox" || typeName == "RadioButton") {
        std::string label;
        if (const PropertyValue* value = component->GetProperty("label")) {
            if (value->Type() == PropertyType::String) {
                label = EvalText(value->AsString());
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
        for (const IntrinsicSize& childBoxSize : childBoxSizes) {
            mainTotal += childBoxSize.width;
            crossMax = std::max(crossMax, childBoxSize.height);
        }
        if (!childBoxSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childBoxSizes.size() - 1);
        }
        size.width = mainTotal + padding.left + padding.right;
        size.height = crossMax + padding.top + padding.bottom;
    } else if (typeName == "Column" || typeName == "For" || typeName == "If") {
        double mainTotal = 0.0;
        double crossMax = 0.0;
        for (const IntrinsicSize& childBoxSize : childBoxSizes) {
            mainTotal += childBoxSize.height;
            crossMax = std::max(crossMax, childBoxSize.width);
        }
        if (!childBoxSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childBoxSizes.size() - 1);
        }
        size.height = mainTotal + padding.top + padding.bottom;
        size.width = crossMax + padding.left + padding.right;
    } else if (typeName == "ScrollView" || typeName == "ListView" || typeName == "Flex") {
        bool isRow = IsHorizontalDirection(component);
        double mainTotal = 0.0;
        double crossMax = 0.0;
        for (const IntrinsicSize& childBoxSize : childBoxSizes) {
            mainTotal += isRow ? childBoxSize.width : childBoxSize.height;
            crossMax = std::max(crossMax, isRow ? childBoxSize.height : childBoxSize.width);
        }
        if (!childBoxSizes.empty()) {
            mainTotal += spacing * static_cast<double>(childBoxSizes.size() - 1);
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
        for (const IntrinsicSize& childBoxSize : childBoxSizes) {
            maxChildWidth = std::max(maxChildWidth, childBoxSize.width);
            maxChildHeight = std::max(maxChildHeight, childBoxSize.height);
        }

        double totalWidth = maxChildWidth * columns + spacing * (columns - 1);
        double totalHeight = maxChildHeight * rows + spacing * (rows - 1);
        size.width = totalWidth + padding.left + padding.right;
        size.height = totalHeight + padding.top + padding.bottom;
    } else {
        double maxWidth = 0.0;
        double maxHeight = 0.0;
        for (const IntrinsicSize& childBoxSize : childBoxSizes) {
            maxWidth = std::max(maxWidth, childBoxSize.width);
            maxHeight = std::max(maxHeight, childBoxSize.height);
        }
        size.width = maxWidth + padding.left + padding.right;
        size.height = maxHeight + padding.top + padding.bottom;
    }

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

    auto intrinsicIt = intrinsic_.find(component->Id());
    const IntrinsicSize intrinsicSize = intrinsicIt != intrinsic_.end() ? intrinsicIt->second : IntrinsicSize{};

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

    // maxHeight caps whatever height was just resolved above -- explicit,
    // stretched, or (the interesting case) intrinsic/content-driven. That
    // last one is what lets a box "grow with its content up to a limit,
    // then scroll": no `height` set at all means `height` above is the
    // content's own intrinsic size (grows freely), and maxHeight then
    // clamps that final box size down. Paired with a ScrollView (which
    // already lays out its children with allowOverflow=true and paints
    // `overflow-y: auto` at whatever final height ends up in this node's
    // rect -- see LayoutNodeRecursive's "ScrollView" branch and
    // SceneCommandWalker's ava-scrollview emission), content shorter than
    // maxHeight sizes the box exactly to itself (no dead space, no
    // scrollbar); content taller than maxHeight stops growing at
    // maxHeight and the browser's native scrollbar takes over -- without
    // this, only a fixed `height` was possible, which is either too tall
    // (empty space) or too short (always scrolling) for variable-length
    // content like a stack trace.
    double maxHeight;
    if (TryReadNumber(component, "maxHeight", &maxHeight)) {
        height = std::min(height, maxHeight);
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
    } else if (typeName == "ScrollView" || typeName == "ListView") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/IsHorizontalDirection(component), /*allowOverflow=*/true);
    } else if (typeName == "Flex") {
        ArrangeRowOrColumn(component, node, contentBox, /*isRow=*/IsHorizontalDirection(component), /*allowOverflow=*/true);
    } else if (typeName == "Grid") {
        ArrangeGrid(component, node, contentBox);
    } else {

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

    const char* mainProp = isRow ? "width" : "height";
    std::vector<double> childMarginMain(children.size(), 0.0);
    std::vector<double> childMainSize(children.size(), -1.0);
    double totalMarginMain = 0.0;
    double fixedTotal = 0.0;
    int autoCount = 0;

    for (size_t i = 0; i < children.size(); ++i) {
        if (IsDialog(children[i])) {
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

    double fixedScale = 1.0;
    if (!allowOverflow && fixedTotal > sizingPool && fixedTotal > 0.0) {
        fixedScale = sizingPool / fixedTotal;
    }

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

        LayoutAlignment hAlign = ReadGridChildAlignment(children[i], "align-h", "width");
        LayoutAlignment vAlign = ReadGridChildAlignment(children[i], "align-v", "height");
        LayoutNodeRecursive(children[i], static_cast<LayoutNode*>(childNodes[i]), cellSlot, hAlign, vAlign);
    }
}

} // namespace layout
} // namespace ui
} // namespace avalang
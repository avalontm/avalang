#include "render_tree/RenderTree.h"
#include "components/IComponent.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "layout/TextMeasure.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

namespace avalang {
namespace ui {
namespace render {

namespace {
// A property reaches RenderTree either as PropertyType::String (a plain
// literal, e.g. text = "hi") or as PropertyType::Expression (a binding
// `{name}` or a `$"...{name}..."` interpolation template -- see
// AvauiPropertyCoercion::InferValueScalarOrList). Both store their raw
// source in the same underlying field, so PropertyValue::AsString() is
// valid and meaningful for either type; every place below that used to
// gate on PropertyType::String alone silently dropped every bound /
// interpolated value (SetText, SetBackgroundColor, etc. never called),
// which is what broke `$"Hola {NAME}"`-style text. IsStringy() is the
// single place that decides which property types carry Eval-able text.
bool IsStringy(PropertyType t) {
    return t == PropertyType::String || t == PropertyType::Expression;
}
}

RenderTree::RenderTree() = default;
RenderTree::~RenderTree() = default;

bool RenderTree::EvalBool(IComponent* comp, const char* propName, bool defaultValue) const {
    const auto* prop = comp->GetProperty(propName);
    if (!prop) return defaultValue;
    if (prop->Type() == PropertyType::Bool) {
        return prop->AsBool();
    }
    if (IsStringy(prop->Type())) {
        return Eval(prop->AsString()) == "true";
    }
    return defaultValue;
}

void RenderTree::CheckBindingWarning(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                      const char* propName) const {
    if (!comp->GetProperty("change")) return;

    const auto* prop = comp->GetProperty(propName);
    if (!prop || !IsStringy(prop->Type()) || prop->AsString().empty()) {
        parent->SetBindingWarning(std::string("No state binding: missing '") + propName +
                                   "' (use " + propName + " = myVariable, declared in state)");
        return;
    }

    const std::string raw = prop->AsString();
    const std::string evaluated = Eval(raw);
    if (evaluated == raw) {
        parent->SetBindingWarning("No state binding: '" + raw + "' is not a state variable (use " +
                                   propName + " = myVariable, declared in state)");
    }
}

double RenderTree::EvalNumber(IComponent* comp, const char* propName, double defaultValue) const {
    const auto* prop = comp->GetProperty(propName);
    if (!prop) return defaultValue;
    if (prop->Type() == PropertyType::Number) {
        return prop->AsNumber();
    }
    if (IsStringy(prop->Type())) {
        std::string evaluated = Eval(prop->AsString());
        char* end = nullptr;
        double parsed = std::strtod(evaluated.c_str(), &end);
        if (end != evaluated.c_str() && *end == '\0') {
            return parsed;
        }
    }
    return defaultValue;
}

void RenderTree::Build(IComponent* componentRoot, LayoutEngine* layoutEngine) {
    if (!componentRoot || !layoutEngine) {
        return;
    }

    nodeMap_.clear();
    dirty_ = false;

    root_ = BuildComponent(componentRoot, layoutEngine);
}

std::shared_ptr<IRenderNode> RenderTree::BuildComponent(IComponent* component,
                                                        LayoutEngine* layoutEngine) {
    if (!component) return nullptr;

    auto layoutNode = layoutEngine->FindNode(component->Id());
    if (!layoutNode) return nullptr;

    std::string typeName = component->TypeName();

    auto renderNode = std::make_shared<RenderNode>(component->Id(), RenderNodeType::Custom);

    renderNode->SetRect(layoutNode->Rect());

    if (const auto* bgColor = component->GetProperty("backgroundColor")) {
        if (IsStringy(bgColor->Type())) {
            renderNode->SetBackgroundColor(Eval(bgColor->AsString()));
            renderNode->SetShouldFill(true);
        }
    }

    if (const auto* borderColor = component->GetProperty("borderColor")) {
        if (IsStringy(borderColor->Type())) {
            renderNode->SetBorderColor(Eval(borderColor->AsString()));
            renderNode->SetShouldStroke(true);
        }
    }

    if (component->GetProperty("borderWidth")) {
        renderNode->SetBorderWidth(static_cast<int>(
            EvalNumber(component, "borderWidth", renderNode->BorderWidth())));
        renderNode->SetShouldStroke(true);
    }

    if (component->GetProperty("borderRadius")) {
        renderNode->SetBorderRadius(static_cast<int>(
            EvalNumber(component, "borderRadius", renderNode->BorderRadius())));
    }

    if (const auto* fgColor = component->GetProperty("textColor")) {
        if (IsStringy(fgColor->Type())) {
            renderNode->SetForegroundColor(Eval(fgColor->AsString()));
        }
    }

    if (const auto* click = component->GetProperty("click")) {
        if (IsStringy(click->Type())) {
            renderNode->SetClickHandler(click->AsString());
        }
    } else if (const auto* change = component->GetProperty("change")) {
        if (IsStringy(change->Type())) {
            renderNode->SetClickHandler(change->AsString());
        }
    }

    if (const auto* cssClass = component->GetProperty("class")) {
        if (IsStringy(cssClass->Type())) {
            renderNode->SetClassName(Eval(cssClass->AsString()));
        }
    }

    if (component->GetProperty("overlay")) {
        renderNode->SetOverlay(EvalBool(component, "overlay", renderNode->IsOverlay()));
    }

    if (component->GetProperty("backdrop")) {
        renderNode->SetBackdrop(EvalBool(component, "backdrop", renderNode->HasBackdrop()));
    }

    if (component->GetProperty("zIndex")) {
        renderNode->SetOverlayPriority(static_cast<int>(
            EvalNumber(component, "zIndex", renderNode->OverlayPriority())));
    }

    if (typeName == "Button") {
        DecomposeButton(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Link") {
        DecomposeLink(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Text" || typeName == "Label") {
        DecomposeText(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Image") {
        DecomposeImage(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Slot") {
        renderNode->SetType(RenderNodeType::Slot);
    } else if (typeName == "TextBox") {
        DecomposeTextBox(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "CheckBox") {
        DecomposeCheckBox(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "RadioButton") {
        DecomposeRadioButton(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "ComboBox") {
        DecomposeComboBox(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Icon") {
        DecomposeIcon(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "Dialog") {
        DecomposeDialog(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else if (typeName == "ScrollView" || typeName == "ListView") {
        DecomposeScrollView(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    } else {
        DecomposeContainer(component, std::static_pointer_cast<RenderNode>(renderNode), layoutEngine);
    }

    nodeMap_[component->Id()] = renderNode;
    return renderNode;
}

void RenderTree::DecomposeButton(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                 LayoutEngine* layout) {
    parent->SetType(RenderNodeType::Button);

    if (parent->BackgroundColor().empty()) {
        parent->SetBackgroundColor("#f0f0f0");
    }
    if (parent->BorderColor().empty()) {
        parent->SetBorderColor("#666666");
    }
    if (parent->BorderWidth() <= 0) {
        parent->SetBorderWidth(1);
    }
    parent->SetShouldFill(true);
    parent->SetShouldStroke(true);

    if (parent->ForegroundColor().empty()) {
        parent->SetForegroundColor("#000000");
    }
    if (const auto* fgColor = comp->GetProperty("textColor")) {
        if (IsStringy(fgColor->Type()) && !fgColor->AsString().empty()) {
            parent->SetForegroundColor(Eval(fgColor->AsString()));
        }
    }
    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || IsStringy(fontSize->Type())) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    } else {
        parent->SetFontSize(12);
    }
    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (IsStringy(fontName->Type())) {
            parent->SetFontName(Eval(fontName->AsString()));
        }
    }

    if (const auto* label = comp->GetProperty("text")) {
        if (IsStringy(label->Type())) {
            parent->SetText(Eval(label->AsString()));
        }
    }

    if (comp->GetProperty("disabled")) {
        parent->SetDisabled(EvalBool(comp, "disabled", parent->Disabled()));
    }

    for (const auto& child : comp->Children()) {
        auto childRender = BuildComponent(child, layout);
        if (childRender) {
            parent->AddChild(childRender);
        }
    }
}

void RenderTree::DecomposeText(IComponent* comp, std::shared_ptr<RenderNode> parent,
                               LayoutEngine* layout) {
    if (const auto* text = comp->GetProperty("text")) {
        if (IsStringy(text->Type())) {
            parent->SetText(Eval(text->AsString()));
            parent->SetType(RenderNodeType::Text);
        }
    }

    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || IsStringy(fontSize->Type())) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    }

    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (IsStringy(fontName->Type())) {
            parent->SetFontName(Eval(fontName->AsString()));
        }
    }

    if (const auto* wrap = comp->GetProperty("wrap")) {
        if (wrap->Type() == PropertyType::Bool) {
            parent->SetWrap(wrap->AsBool());
        }
    }
}

void RenderTree::DecomposeLink(IComponent* comp, std::shared_ptr<RenderNode> parent,
                               LayoutEngine* layout) {
    parent->SetType(RenderNodeType::Link);

    if (const auto* text = comp->GetProperty("text")) {
        if (IsStringy(text->Type())) {
            parent->SetText(Eval(text->AsString()));
        }
    }

    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || IsStringy(fontSize->Type())) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    }

    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (IsStringy(fontName->Type())) {
            parent->SetFontName(Eval(fontName->AsString()));
        }
    }

    if (const auto* href = comp->GetProperty("href")) {
        if (IsStringy(href->Type())) {
            parent->SetHref(Eval(href->AsString()));
        }
    }
}

void RenderTree::DecomposeImage(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                LayoutEngine* layout) {
    if (const auto* src = comp->GetProperty("source")) {
        if (IsStringy(src->Type())) {
            parent->SetImagePath(Eval(src->AsString()));
            parent->SetType(RenderNodeType::Image);
        }
    }
}

void RenderTree::DecomposeTextBox(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                  LayoutEngine* layout) {
    parent->SetType(RenderNodeType::Input);

    std::string text;
    if (const auto* value = comp->GetProperty("text")) {
        if (IsStringy(value->Type())) {
            text = Eval(value->AsString());
        }
    }
    parent->SetText(text);

    std::string placeholder;
    if (const auto* ph = comp->GetProperty("placeholder")) {
        if (IsStringy(ph->Type())) {
            placeholder = Eval(ph->AsString());
        }
    }
    parent->SetOptionsData(placeholder);

    if (parent->ForegroundColor().empty()) {
        parent->SetForegroundColor("#000000");
    }

    if (comp->GetProperty("isEnabled")) {
        parent->SetDisabled(!EvalBool(comp, "isEnabled", !parent->Disabled()));
    }

    const int caret = static_cast<int>(EvalNumber(comp, "caretIndex", -1.0));
    const int anchor = static_cast<int>(EvalNumber(comp, "selectionAnchor", static_cast<double>(caret)));
    parent->SetCaretIndex(caret);
    parent->SetSelectionRange(std::min(caret, anchor), std::max(caret, anchor));

    if (const auto* composition = comp->GetProperty("imeComposition")) {
        if (IsStringy(composition->Type())) {
            parent->SetImeComposition(Eval(composition->AsString()));
        }
    }
    parent->SetImeCompositionCursor(static_cast<int>(EvalNumber(comp, "imeCompositionCursor", 0.0)));

    CheckBindingWarning(comp, parent, "text");
}

void RenderTree::DecomposeCheckBox(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                   LayoutEngine* layout) {
    parent->SetType(RenderNodeType::Checkbox);

    bool isChecked = EvalBool(comp, "isChecked", false);

    LayoutRect boxRect = parent->Rect();
    const double boxSize = std::min(16.0, boxRect.height > 0 ? boxRect.height : 16.0);
    boxRect.width = boxSize;
    boxRect.height = boxSize;

    auto boxNode = std::make_shared<RenderNode>(comp->Id(), RenderNodeType::Rectangle);
    boxNode->SetRect(boxRect);
    boxNode->SetBorderColor(parent->BorderColor().empty() ? "#666666" : parent->BorderColor());
    boxNode->SetBorderWidth(parent->BorderWidth() > 0 ? parent->BorderWidth() : 1);
    boxNode->SetShouldStroke(true);
    if (isChecked) {
        boxNode->SetBackgroundColor("#0078D4");
        boxNode->SetShouldFill(true);
    }
    boxNode->SetClickHandler(parent->ClickHandler());
    parent->AddChild(boxNode);
    parent->SetShouldFill(false);
    parent->SetShouldStroke(false);

    if (const auto* label = comp->GetProperty("label")) {
        if (IsStringy(label->Type()) && !label->AsString().empty()) {
            auto textNode = std::make_shared<RenderNode>(comp->Id(), RenderNodeType::Text);
            const std::string text = Eval(label->AsString());
            const double fontSize = 12.0;
            textNode->SetText(text);
            textNode->SetForegroundColor(parent->ForegroundColor().empty() ? "#000000" : parent->ForegroundColor());
            textNode->SetFontSize(static_cast<int>(fontSize));
            textNode->SetClickHandler(parent->ClickHandler());
            LayoutRect labelRect = parent->Rect();
            labelRect.x += boxSize + 6.0;
            labelRect.y += std::max(0.0, (parent->Rect().height - layout::DefaultLineHeight(fontSize)) / 2.0);
            textNode->SetRect(labelRect);
            parent->AddChild(textNode);
        }
    }

    CheckBindingWarning(comp, parent, "isChecked");
}

void RenderTree::DecomposeRadioButton(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                      LayoutEngine* layout) {
    parent->SetType(RenderNodeType::RadioButton);

    bool isSelected = EvalBool(comp, "isSelected", false);

    LayoutRect boxRect = parent->Rect();
    const double boxSize = std::min(16.0, boxRect.height > 0 ? boxRect.height : 16.0);
    boxRect.width = boxSize;
    boxRect.height = boxSize;

    auto boxNode = std::make_shared<RenderNode>(comp->Id(), RenderNodeType::Ellipse);
    boxNode->SetRect(boxRect);
    boxNode->SetBorderColor(parent->BorderColor().empty() ? "#666666" : parent->BorderColor());
    boxNode->SetBorderWidth(parent->BorderWidth() > 0 ? parent->BorderWidth() : 1);
    boxNode->SetShouldStroke(true);
    if (isSelected) {
        boxNode->SetBackgroundColor("#0078D4");
        boxNode->SetShouldFill(true);
    }
    boxNode->SetClickHandler(parent->ClickHandler());
    parent->AddChild(boxNode);
    parent->SetShouldFill(false);
    parent->SetShouldStroke(false);

    if (const auto* label = comp->GetProperty("label")) {
        if (IsStringy(label->Type()) && !label->AsString().empty()) {
            auto textNode = std::make_shared<RenderNode>(comp->Id(), RenderNodeType::Text);
            const std::string text = Eval(label->AsString());
            const double fontSize = 12.0;
            textNode->SetText(text);
            textNode->SetForegroundColor(parent->ForegroundColor().empty() ? "#000000" : parent->ForegroundColor());
            textNode->SetFontSize(static_cast<int>(fontSize));
            textNode->SetClickHandler(parent->ClickHandler());
            LayoutRect labelRect = parent->Rect();
            labelRect.x += boxSize + 6.0;
            labelRect.y += std::max(0.0, (parent->Rect().height - layout::DefaultLineHeight(fontSize)) / 2.0);
            textNode->SetRect(labelRect);
            parent->AddChild(textNode);
        }
    }

    CheckBindingWarning(comp, parent, "isSelected");
}

void RenderTree::DecomposeComboBox(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                   LayoutEngine* layout) {
    parent->SetType(RenderNodeType::ComboBox);

    if (const auto* isOpen = comp->GetProperty("isOpen")) {
        if (isOpen->Type() == PropertyType::Bool) {
            parent->SetOpen(isOpen->AsBool());
        }
    }

    std::string selectedValue;
    if (const auto* value = comp->GetProperty("selectedValue")) {
        if (IsStringy(value->Type())) {
            selectedValue = Eval(value->AsString());
        }
    }

    ComboBoxItems items;
    for (IComponent* child : comp->Children()) {
        const auto* valueProp = child->GetProperty("value");
        const auto* labelProp = child->GetProperty("label");
        std::string v = (valueProp && IsStringy(valueProp->Type())) ? valueProp->AsString() : "";
        std::string l = (labelProp && IsStringy(labelProp->Type())) ? labelProp->AsString() : "";

        items.push_back(ComboBoxItem{v, l, v == selectedValue});
    }
    parent->SetComboItems(std::move(items));

    CheckBindingWarning(comp, parent, "selectedValue");
}

void RenderTree::DecomposeIcon(IComponent* comp, std::shared_ptr<RenderNode> parent,
                               LayoutEngine* layout) {
    if (const auto* src = comp->GetProperty("source")) {
        if (IsStringy(src->Type())) {
            parent->SetImagePath(Eval(src->AsString()));
        }
    }
    parent->SetType(RenderNodeType::Icon);
}

void RenderTree::DecomposeDialog(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                 LayoutEngine* layout) {
    parent->SetType(RenderNodeType::Dialog);

    bool isOpen = EvalBool(comp, "isOpen", false);

    if (const auto* title = comp->GetProperty("title")) {
        if (IsStringy(title->Type())) {
            parent->SetText(Eval(title->AsString()));
        }
    }

    if (!isOpen) {
        parent->SetOverlay(false);
        parent->SetBackdrop(false);
        parent->SetShouldFill(false);
        parent->SetShouldStroke(false);
        return;
    }

    if (parent->BackgroundColor().empty()) {
        parent->SetBackgroundColor("#ffffff");
        parent->SetShouldFill(true);
    }
    if (parent->BorderRadius() <= 0) {
        parent->SetBorderRadius(8);
    }

    for (const auto& child : comp->Children()) {
        auto childRender = BuildComponent(child, layout);
        if (childRender) {
            parent->AddChild(childRender);
        }
    }
}

void RenderTree::DecomposeScrollView(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                     LayoutEngine* layout) {
    parent->SetType(RenderNodeType::ScrollView);

    if (const auto* direction = comp->GetProperty("direction")) {
        if (IsStringy(direction->Type()) && !direction->AsString().empty()) {
            parent->SetScrollDirection(Eval(direction->AsString()));
        }
    }

    double scrollOffsetX = 0.0;
    if (const auto* offsetX = comp->GetProperty("scrollOffsetX")) {
        if (offsetX->Type() == PropertyType::Number) {
            scrollOffsetX = offsetX->AsNumber();
        }
    }

    double scrollOffsetY = 0.0;
    if (const auto* offsetY = comp->GetProperty("scrollOffsetY")) {
        if (offsetY->Type() == PropertyType::Number) {
            scrollOffsetY = offsetY->AsNumber();
        }
    }

    parent->SetScrollOffset(scrollOffsetX, scrollOffsetY);

    for (const auto& child : comp->Children()) {
        auto childRender = BuildComponent(child, layout);
        if (childRender) {
            parent->AddChild(childRender);
        }
    }
}

void RenderTree::DecomposeContainer(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                    LayoutEngine* layout) {
    std::string typeName = comp->TypeName();

    if (typeName == "Row") {
        parent->SetType(RenderNodeType::Row);
    } else if (typeName == "Column") {
        parent->SetType(RenderNodeType::Column);
    } else if (typeName == "Stack") {
        parent->SetType(RenderNodeType::Stack);
    } else {
        parent->SetType(RenderNodeType::Container);
    }

    if (const auto* bgColor = comp->GetProperty("backgroundColor")) {
        if (IsStringy(bgColor->Type())) {
            parent->SetBackgroundColor(Eval(bgColor->AsString()));
            parent->SetShouldFill(true);
        }
    }

    for (const auto& child : comp->Children()) {
        auto childRender = BuildComponent(child, layout);
        if (childRender) {
            parent->AddChild(childRender);
        }
    }
}

std::shared_ptr<IRenderNode> RenderTree::FindNode(ComponentId componentId) const {
    auto it = nodeMap_.find(componentId);
    if (it != nodeMap_.end()) {
        return it->second;
    }
    return nullptr;
}

void RenderTree::ForEach(std::function<void(const std::shared_ptr<IRenderNode>&)> visitor) {
    if (root_) {
        ForEachRecursive(root_, visitor);
    }
}

void RenderTree::ForEachRecursive(const std::shared_ptr<IRenderNode>& node,
                                  std::function<void(const std::shared_ptr<IRenderNode>&)> visitor) {
    if (!node) return;
    visitor(node);
    for (const auto& child : node->Children()) {
        ForEachRecursive(child, visitor);
    }
}

}
}
}
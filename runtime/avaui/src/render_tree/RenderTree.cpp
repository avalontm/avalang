#include "render_tree/RenderTree.h"
#include "components/IComponent.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "layout/TextMeasure.h"
#include <functional>
#include <algorithm>
#include <cstdlib>

namespace avalang {
namespace ui {
namespace render {

RenderTree::RenderTree() = default;
RenderTree::~RenderTree() = default;

bool RenderTree::EvalBool(IComponent* comp, const char* propName, bool defaultValue) const {
    const auto* prop = comp->GetProperty(propName);
    if (!prop) return defaultValue;
    if (prop->Type() == PropertyType::Bool) {
        return prop->AsBool();
    }
    if (prop->Type() == PropertyType::String) {
        return Eval(prop->AsString()) == "true";
    }
    return defaultValue;
}

void RenderTree::CheckBindingWarning(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                      const char* propName) const {
    // Only controls wired for interactivity need two-way binding --
    // one with no `change` handler at all is a legitimate static/
    // display-only control, nothing to warn about.
    if (!comp->GetProperty("change")) return;

    const auto* prop = comp->GetProperty(propName);
    if (!prop || prop->Type() != PropertyType::String || prop->AsString().empty()) {
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
    if (prop->Type() == PropertyType::String) {
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
    lastLayoutEngine_ = layoutEngine;

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
        if (bgColor->Type() == PropertyType::String) {
            renderNode->SetBackgroundColor(Eval(bgColor->AsString()));
            renderNode->SetShouldFill(true);
        }
    }

    if (const auto* borderColor = component->GetProperty("borderColor")) {
        if (borderColor->Type() == PropertyType::String) {
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
        if (fgColor->Type() == PropertyType::String) {
            renderNode->SetForegroundColor(Eval(fgColor->AsString()));
        }
    } else if (const auto* legacyColor = component->GetProperty("color")) {
        if (legacyColor->Type() == PropertyType::String) {
            renderNode->SetForegroundColor(Eval(legacyColor->AsString()));
        }
    }

    if (const auto* click = component->GetProperty("click")) {
        if (click->Type() == PropertyType::String) {
            renderNode->SetClickHandler(click->AsString());
        }
    } else if (const auto* change = component->GetProperty("change")) {
        if (change->Type() == PropertyType::String) {
            renderNode->SetClickHandler(change->AsString());
        }
    }

    if (const auto* cssClass = component->GetProperty("class")) {
        if (cssClass->Type() == PropertyType::String) {
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
        if (fgColor->Type() == PropertyType::String && !fgColor->AsString().empty()) {
            parent->SetForegroundColor(Eval(fgColor->AsString()));
        }
    } else if (const auto* legacyColor = comp->GetProperty("color")) {
        if (legacyColor->Type() == PropertyType::String && !legacyColor->AsString().empty()) {
            parent->SetForegroundColor(Eval(legacyColor->AsString()));
        }
    }
    // fontSize/fontName: same pattern as DecomposeText below -- read
    // whatever RenderTheme::Apply already resolved onto the component
    // (theme default, or a project override from app.ava) and copy it
    // onto the RenderNode. Previously this unconditionally hardcoded
    // SetFontSize(12) and never called SetFontName at all, so a Button
    // always painted with RenderNode's own "Arial" default regardless
    // of theme/app.ava -- the one control type that silently ignored
    // both the theme's button font size and any custom font.
    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || fontSize->Type() == PropertyType::String) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    } else {
        parent->SetFontSize(12);
    }
    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (fontName->Type() == PropertyType::String) {
            parent->SetFontName(Eval(fontName->AsString()));
        }
    }

    if (const auto* label = comp->GetProperty("text")) {
        if (label->Type() == PropertyType::String) {
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
        if (text->Type() == PropertyType::String) {
            parent->SetText(Eval(text->AsString()));
            parent->SetType(RenderNodeType::Text);
        }
    }

    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || fontSize->Type() == PropertyType::String) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    }

    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (fontName->Type() == PropertyType::String) {
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
        if (text->Type() == PropertyType::String) {
            parent->SetText(Eval(text->AsString()));
        }
    }

    if (const auto* fontSize = comp->GetProperty("fontSize")) {
        if (fontSize->Type() == PropertyType::Number || fontSize->Type() == PropertyType::String) {
            parent->SetFontSize(static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize())));
        }
    }

    if (const auto* fontName = comp->GetProperty("fontName")) {
        if (fontName->Type() == PropertyType::String) {
            parent->SetFontName(Eval(fontName->AsString()));
        }
    }

    if (const auto* href = comp->GetProperty("href")) {
        if (href->Type() == PropertyType::String) {
            parent->SetHref(Eval(href->AsString()));
        }
    }
}

void RenderTree::DecomposeImage(IComponent* comp, std::shared_ptr<RenderNode> parent,
                                LayoutEngine* layout) {
    if (const auto* src = comp->GetProperty("source")) {
        if (src->Type() == PropertyType::String) {
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
        if (value->Type() == PropertyType::String) {
            text = Eval(value->AsString());
        }
    }
    // Actual value on Text(); placeholder reuses the OptionsData slot
    // (generic string field, otherwise only used by ComboBox) so no
    // interface change is needed to carry a second string.
    parent->SetText(text);

    std::string placeholder;
    if (const auto* ph = comp->GetProperty("placeholder")) {
        if (ph->Type() == PropertyType::String) {
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
    // The parent node (Checkbox type, ShouldFill/ShouldStroke both false
    // below) never draws anything itself -- its `change`-derived
    // ClickHandler (set earlier in BuildComponent) would go nowhere
    // unless propagated onto the actual elements the walker draws: the
    // box and, if present, the label. Without this, clicking either one
    // emits no data-event/data-handler at all and the control is
    // permanently inert, `change` binding notwithstanding.
    boxNode->SetClickHandler(parent->ClickHandler());
    parent->AddChild(boxNode);
    parent->SetShouldFill(false);
    parent->SetShouldStroke(false);

    if (const auto* label = comp->GetProperty("label")) {
        if (label->Type() == PropertyType::String && !label->AsString().empty()) {
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
    parent->SetType(RenderNodeType::Custom);

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
    // See DecomposeCheckBox's identical comment: the parent's
    // `change`-derived ClickHandler goes nowhere unless propagated onto
    // the elements actually drawn (box + label), or the control never
    // responds to a click at all.
    boxNode->SetClickHandler(parent->ClickHandler());
    parent->AddChild(boxNode);
    parent->SetShouldFill(false);
    parent->SetShouldStroke(false);

    if (const auto* label = comp->GetProperty("label")) {
        if (label->Type() == PropertyType::String && !label->AsString().empty()) {
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

    std::string selectedValue;
    if (const auto* value = comp->GetProperty("selectedValue")) {
        if (value->Type() == PropertyType::String) {
            selectedValue = Eval(value->AsString());
        }
    }

    std::string optionsData;
    for (IComponent* child : comp->Children()) {
        const auto* valueProp = child->GetProperty("value");
        const auto* labelProp = child->GetProperty("label");
        std::string v = (valueProp && valueProp->Type() == PropertyType::String) ? valueProp->AsString() : "";
        std::string l = (labelProp && labelProp->Type() == PropertyType::String) ? labelProp->AsString() : "";

        if (!optionsData.empty()) optionsData += ";;";
        optionsData += v + "|" + l + "|" + (v == selectedValue ? "1" : "0");
    }
    parent->SetOptionsData(optionsData);

    CheckBindingWarning(comp, parent, "selectedValue");
}

void RenderTree::DecomposeIcon(IComponent* comp, std::shared_ptr<RenderNode> parent,
                               LayoutEngine* layout) {
    // Same contract as DecomposeImage: `source` is copied through as-is,
    // resolution of @local/@icons/@fonts prefixes lives in the renderer
    // (see ResourcePathResolver), not here.
    if (const auto* src = comp->GetProperty("source")) {
        if (src->Type() == PropertyType::String) {
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
        if (title->Type() == PropertyType::String) {
            parent->SetText(Eval(title->AsString()));
        }
    }

    if (!isOpen) {
        // Closed dialog: no children, nothing painted. The generic
        // property parsing in BuildComponent already ran with
        // RenderTheme's type="dialog" defaults (overlay=true,
        // backdrop=true, plus surface backgroundColor/borderColor/
        // borderWidth/borderRadius) before this function was called --
        // undo all of it here. Clearing overlay/backdrop alone isn't
        // enough: BuildComponent also saw backgroundColor/borderColor
        // as real properties on the component and already called
        // SetShouldFill(true)/SetShouldStroke(true) on `parent`, so a
        // closed dialog still hit SceneCommandWalker's generic
        // ShouldFill()/ShouldStroke() draw path and painted its surface
        // rectangle (theme surface/border colors) sitting inline in the
        // layout -- exactly the empty bordered box that showed up in
        // place of the dialog when isOpen=false.
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
        if (direction->Type() == PropertyType::String && !direction->AsString().empty()) {
            parent->SetScrollDirection(Eval(direction->AsString()));
        }
    }

    // backgroundColor/borderColor/etc. were already read generically in
    // BuildComponent above -- children are arranged by LayoutEngine
    // exactly like a Column/Row (see LayoutEngineImpl.cpp's "ScrollView"
    // branch), so their rects can legitimately extend past `parent`'s own
    // Rect() on the scrolling axis. SceneCommandWalker is what turns that
    // overflow into an actual native-scrolling region on web (see its
    // "ScrollView" case) -- this function only builds the tree.
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
        if (bgColor->Type() == PropertyType::String) {
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

} // namespace render
} // namespace ui
} // namespace avalang

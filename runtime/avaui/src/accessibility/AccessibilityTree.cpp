#include "accessibility/AccessibilityTree.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace avalang {
namespace ui {
namespace accessibility {

namespace {

std::string Lowercase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

AccessibilityRole RoleForType(const std::string& typeLower) {
    if (typeLower == "button") return AccessibilityRole::Button;
    if (typeLower == "text" || typeLower == "label") return AccessibilityRole::Text;
    if (typeLower == "image") return AccessibilityRole::Image;
    if (typeLower == "textbox" || typeLower == "textinput" || typeLower == "input") {
        return AccessibilityRole::TextInput;
    }
    if (typeLower == "checkbox") return AccessibilityRole::CheckBox;
    if (typeLower == "radiobutton" || typeLower == "radio") return AccessibilityRole::RadioButton;
    if (typeLower == "combobox") return AccessibilityRole::ComboBox;
    if (typeLower == "dialog") return AccessibilityRole::Dialog;
    if (typeLower == "link") return AccessibilityRole::Link;
    if (typeLower == "scrollview" || typeLower == "scroll") return AccessibilityRole::ScrollView;
    if (typeLower == "page") return AccessibilityRole::Page;
    return AccessibilityRole::Container;
}

bool AsBool(const IComponent* component, const std::string& name) {
    const PropertyValue* value = component->GetProperty(name);
    return value != nullptr && value->Type() == PropertyType::Bool && value->AsBool();
}

bool AsString(const IComponent* component, const std::string& name, std::string& out) {
    const PropertyValue* value = component->GetProperty(name);
    if (!value || value->Type() != PropertyType::String) return false;
    out = value->AsString();
    return true;
}

void ApplyDefaultActions(AccessibilityRole role, AccessibilityNode* node) {
    switch (role) {
        case AccessibilityRole::Button:
        case AccessibilityRole::Link:
        case AccessibilityRole::CheckBox:
        case AccessibilityRole::RadioButton:
        case AccessibilityRole::ComboBox:
            node->AddAction(AccessibilityAction::Activate);
            break;
        case AccessibilityRole::TextInput:
            node->AddAction(AccessibilityAction::SetValue);
            break;
        case AccessibilityRole::ScrollView:
            node->AddAction(AccessibilityAction::Scroll);
            break;
        case AccessibilityRole::Dialog:
            node->AddAction(AccessibilityAction::Dismiss);
            break;
        default:
            break;
    }
}

}

std::unique_ptr<AccessibilityTree> AccessibilityTree::Create(ComponentTree* componentTree) {
    std::unique_ptr<AccessibilityTree> tree(new AccessibilityTree());
    tree->Build(componentTree);
    return tree;
}

AccessibilityNode* AccessibilityTree::Root() const {
    return root_;
}

const std::vector<std::unique_ptr<AccessibilityNode>>& AccessibilityTree::Nodes() const {
    return nodes_;
}

AccessibilityNode* AccessibilityTree::FindById(ComponentId id) const {
    for (const std::unique_ptr<AccessibilityNode>& node : nodes_) {
        if (node->Id() == id) return node.get();
    }
    return nullptr;
}

void AccessibilityTree::Build(ComponentTree* componentTree) {
    if (!componentTree) return;
    IComponent* rootComponent = componentTree->Root();
    if (!rootComponent) return;
    root_ = BuildNode(rootComponent, nullptr);
}

AccessibilityNode* AccessibilityTree::BuildNode(IComponent* component, AccessibilityNode* parent) {
    if (!component) return nullptr;

    const std::string typeLower = Lowercase(component->TypeName());
    AccessibilityRole role = RoleForType(typeLower);
    std::unique_ptr<AccessibilityNode> node(new AccessibilityNode(component->Id(), role));

    std::string label;
    if (AsString(component, "label", label) || AsString(component, "text", label)) {
        node->SetLabel(label);
    }
    std::string value;
    if (AsString(component, "value", value)) {
        node->SetValue(value);
    }
    std::string hint;
    if (AsString(component, "placeholder", hint)) {
        node->SetHint(hint);
    }

    node->AddState(AccessibilityState::Focusable);
    if (AsBool(component, "disabled")) {
        node->AddState(AccessibilityState::Disabled);
        node->RemoveState(AccessibilityState::Focusable);
    }
    if (AsBool(component, "visible")) {
        node->AddState(AccessibilityState::Visible);
    } else {
        node->AddState(AccessibilityState::Hidden);
    }
    if (role == AccessibilityRole::CheckBox || role == AccessibilityRole::RadioButton) {
        if (AsBool(component, "checked") || AsBool(component, "isChecked")) {
            node->AddState(AccessibilityState::Checked);
        }
    }
    if (AsBool(component, "isSelected")) {
        node->AddState(AccessibilityState::Selected);
    }

    ApplyDefaultActions(role, node.get());

    AccessibilityNode* raw = node.get();
    raw->SetParent(parent);
    nodes_.push_back(std::move(node));

    if (parent) {
        parent->AddChild(raw);
    }

    for (IComponent* child : component->Children()) {
        BuildNode(child, raw);
    }

    return raw;
}

}
}
}
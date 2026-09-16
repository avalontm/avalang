#include "accessibility/AccessibilityMappings.h"

namespace avalang {
namespace ui {
namespace accessibility {

WindowsMapping MapRoleToWindows(AccessibilityRole role) {
    WindowsMapping mapping;
    switch (role) {
        case AccessibilityRole::Button:
            mapping.controlType = "UIA_ButtonControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Text:
            mapping.controlType = "UIA_TextControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Image:
            mapping.controlType = "UIA_ImageControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Container:
            mapping.controlType = "UIA_GroupControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::TextInput:
            mapping.controlType = "UIA_EditControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::CheckBox:
            mapping.controlType = "UIA_CheckBoxControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::RadioButton:
            mapping.controlType = "UIA_RadioButtonControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::ComboBox:
            mapping.controlType = "UIA_ComboBoxControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Dialog:
            mapping.controlType = "UIA_WindowControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Link:
            mapping.controlType = "UIA_HyperlinkControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::ScrollView:
            mapping.controlType = "UIA_ScrollBarControlTypeId";
            mapping.automationId = "id";
            break;
        case AccessibilityRole::Page:
            mapping.controlType = "UIA_DocumentControlTypeId";
            mapping.automationId = "id";
            break;
        default:
            mapping.controlType = "UIA_CustomControlTypeId";
            mapping.automationId = "id";
            break;
    }
    return mapping;
}

AriaMapping MapRoleToAria(AccessibilityRole role) {
    AriaMapping mapping;
    mapping.labelProperty = "aria-label";
    mapping.valueProperty = "aria-valuetext";
    mapping.checkedProperty = "aria-checked";
    mapping.selectedProperty = "aria-selected";
    mapping.disabledProperty = "aria-disabled";
    mapping.expandedProperty = "aria-expanded";
    switch (role) {
        case AccessibilityRole::Button:
            mapping.role = "button";
            break;
        case AccessibilityRole::Text:
            mapping.role = "text";
            break;
        case AccessibilityRole::Image:
            mapping.role = "img";
            break;
        case AccessibilityRole::Container:
            mapping.role = "group";
            break;
        case AccessibilityRole::TextInput:
            mapping.role = "textbox";
            break;
        case AccessibilityRole::CheckBox:
            mapping.role = "checkbox";
            break;
        case AccessibilityRole::RadioButton:
            mapping.role = "radio";
            break;
        case AccessibilityRole::ComboBox:
            mapping.role = "combobox";
            break;
        case AccessibilityRole::Dialog:
            mapping.role = "dialog";
            break;
        case AccessibilityRole::Link:
            mapping.role = "link";
            break;
        case AccessibilityRole::ScrollView:
            mapping.role = "scrollbar";
            break;
        case AccessibilityRole::Page:
            mapping.role = "document";
            break;
        default:
            mapping.role = "generic";
            break;
    }
    return mapping;
}

AndroidMapping MapRoleToAndroid(AccessibilityRole role) {
    AndroidMapping mapping;
    mapping.contentDescriptionProperty = "contentDescription";
    mapping.checkedProperty = "checked";
    mapping.selectedProperty = "selected";
    mapping.enabledProperty = "enabled";
    switch (role) {
        case AccessibilityRole::Button:
            mapping.viewClassName = "Button";
            mapping.role = "button";
            break;
        case AccessibilityRole::Text:
            mapping.viewClassName = "TextView";
            mapping.role = "text";
            break;
        case AccessibilityRole::Image:
            mapping.viewClassName = "ImageView";
            mapping.role = "image";
            break;
        case AccessibilityRole::Container:
            mapping.viewClassName = "ViewGroup";
            mapping.role = "container";
            break;
        case AccessibilityRole::TextInput:
            mapping.viewClassName = "EditText";
            mapping.role = "textbox";
            break;
        case AccessibilityRole::CheckBox:
            mapping.viewClassName = "CheckBox";
            mapping.role = "checkbox";
            break;
        case AccessibilityRole::RadioButton:
            mapping.viewClassName = "RadioButton";
            mapping.role = "radio";
            break;
        case AccessibilityRole::ComboBox:
            mapping.viewClassName = "Spinner";
            mapping.role = "combobox";
            break;
        case AccessibilityRole::Dialog:
            mapping.viewClassName = "Dialog";
            mapping.role = "dialog";
            break;
        case AccessibilityRole::Link:
            mapping.viewClassName = "TextView";
            mapping.role = "link";
            break;
        case AccessibilityRole::ScrollView:
            mapping.viewClassName = "ScrollView";
            mapping.role = "scroll";
            break;
        case AccessibilityRole::Page:
            mapping.viewClassName = "FrameLayout";
            mapping.role = "page";
            break;
        default:
            mapping.viewClassName = "View";
            mapping.role = "generic";
            break;
    }
    return mapping;
}

IosMapping MapRoleToIos(AccessibilityRole role) {
    IosMapping mapping;
    mapping.labelProperty = "accessibilityLabel";
    mapping.valueProperty = "accessibilityValue";
    mapping.hintProperty = "accessibilityHint";
    switch (role) {
        case AccessibilityRole::Button:
            mapping.trait = "UIAccessibilityTraitButton";
            break;
        case AccessibilityRole::Text:
            mapping.trait = "UIAccessibilityTraitStaticText";
            break;
        case AccessibilityRole::Image:
            mapping.trait = "UIAccessibilityTraitImage";
            break;
        case AccessibilityRole::Container:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::TextInput:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::CheckBox:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::RadioButton:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::ComboBox:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::Dialog:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::Link:
            mapping.trait = "UIAccessibilityTraitLink";
            break;
        case AccessibilityRole::ScrollView:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        case AccessibilityRole::Page:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
        default:
            mapping.trait = "UIAccessibilityTraitNone";
            break;
    }
    return mapping;
}

const char* ActionToString(AccessibilityAction action) {
    switch (action) {
        case AccessibilityAction::Activate: return "activate";
        case AccessibilityAction::Focus: return "focus";
        case AccessibilityAction::SetValue: return "set_value";
        case AccessibilityAction::Scroll: return "scroll";
        case AccessibilityAction::Dismiss: return "dismiss";
        default: return "unknown";
    }
}

const char* RoleToString(AccessibilityRole role) {
    switch (role) {
        case AccessibilityRole::Button: return "button";
        case AccessibilityRole::Text: return "text";
        case AccessibilityRole::Image: return "image";
        case AccessibilityRole::Container: return "container";
        case AccessibilityRole::TextInput: return "textinput";
        case AccessibilityRole::CheckBox: return "checkbox";
        case AccessibilityRole::RadioButton: return "radiobutton";
        case AccessibilityRole::ComboBox: return "combobox";
        case AccessibilityRole::Dialog: return "dialog";
        case AccessibilityRole::Link: return "link";
        case AccessibilityRole::ScrollView: return "scrollview";
        case AccessibilityRole::Page: return "page";
        default: return "unknown";
    }
}

}
}
}
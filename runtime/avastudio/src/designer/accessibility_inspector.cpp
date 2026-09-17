#include "designer/accessibility_inspector.h"

namespace studio::designer {

namespace {

bool IsInteractiveRole(avalang::ui::accessibility::AccessibilityRole role) {
    using avalang::ui::accessibility::AccessibilityRole;
    switch (role) {
        case AccessibilityRole::Button:
        case AccessibilityRole::TextInput:
        case AccessibilityRole::CheckBox:
        case AccessibilityRole::RadioButton:
        case AccessibilityRole::ComboBox:
        case AccessibilityRole::Link:
            return true;
        default:
            return false;
    }
}

}

std::unique_ptr<avalang::ui::accessibility::AccessibilityTree> BuildAccessibilityTree(UiComponentTree* tree) {
    return avalang::ui::accessibility::AccessibilityTree::Create(tree);
}

std::vector<AccessibilityDiagnostic> MissingLabelDiagnostics(
    const avalang::ui::accessibility::AccessibilityTree& tree) {
    std::vector<AccessibilityDiagnostic> diagnostics;

    for (const std::unique_ptr<avalang::ui::accessibility::AccessibilityNode>& node : tree.Nodes()) {
        if (!node || !IsInteractiveRole(node->Role())) {
            continue;
        }
        if (node->Label().empty()) {
            diagnostics.push_back({node->Id(), "Missing accessible label"});
        }
    }

    return diagnostics;
}

std::vector<avalang::ui::accessibility::AccessibilityNode*> TabOrder(
    const avalang::ui::accessibility::AccessibilityTree& tree) {
    std::vector<avalang::ui::accessibility::AccessibilityNode*> order;

    for (const std::unique_ptr<avalang::ui::accessibility::AccessibilityNode>& node : tree.Nodes()) {
        if (node && node->HasState(avalang::ui::accessibility::AccessibilityState::Focusable) &&
            !node->HasState(avalang::ui::accessibility::AccessibilityState::Disabled) &&
            !node->HasState(avalang::ui::accessibility::AccessibilityState::Hidden)) {
            order.push_back(node.get());
        }
    }

    return order;
}

double TextContrastRatio(const avalang::ui::ThemeColor& textColor, const avalang::ui::ThemeColor& backgroundColor) {
    return ContrastRatio(textColor, backgroundColor);
}

bool MeetsWcagAA(double contrastRatio, bool isLargeText) {
    return contrastRatio >= (isLargeText ? 3.0 : 4.5);
}

}

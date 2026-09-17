#pragma once

#include <memory>
#include <string>
#include <vector>

#include "accessibility/AccessibilityTree.h"
#include "designer/theme_tokens.h"
#include "designer/types.h"

namespace studio::designer {

std::unique_ptr<avalang::ui::accessibility::AccessibilityTree> BuildAccessibilityTree(UiComponentTree* tree);

struct AccessibilityDiagnostic {
    UiComponentId nodeId;
    std::string message;
};

std::vector<AccessibilityDiagnostic> MissingLabelDiagnostics(
    const avalang::ui::accessibility::AccessibilityTree& tree);

std::vector<avalang::ui::accessibility::AccessibilityNode*> TabOrder(
    const avalang::ui::accessibility::AccessibilityTree& tree);

double TextContrastRatio(const avalang::ui::ThemeColor& textColor, const avalang::ui::ThemeColor& backgroundColor);

bool MeetsWcagAA(double contrastRatio, bool isLargeText = false);

}

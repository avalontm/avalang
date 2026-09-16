#pragma once

#include "accessibility/AccessibilityTree.h"
#include "accessibility/AccessibilityMappings.h"

#include <string>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidAccessibility_SetTree(accessibility::AccessibilityTree* tree);

int AndroidAccessibility_NodeCount();
accessibility::AccessibilityNode* AndroidAccessibility_NodeAt(int index);
accessibility::AndroidMapping AndroidAccessibility_MappingFor(const accessibility::AccessibilityNode* node);

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang

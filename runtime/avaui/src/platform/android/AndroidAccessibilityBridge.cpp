#include "AndroidAccessibilityBridge.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

accessibility::AccessibilityTree* g_tree = nullptr;

}

void AndroidAccessibility_SetTree(accessibility::AccessibilityTree* tree) {
    g_tree = tree;
}

int AndroidAccessibility_NodeCount() {
    if (!g_tree) return 0;
    return static_cast<int>(g_tree->Nodes().size());
}

accessibility::AccessibilityNode* AndroidAccessibility_NodeAt(int index) {
    if (!g_tree) return nullptr;
    const auto& nodes = g_tree->Nodes();
    if (index < 0 || static_cast<size_t>(index) >= nodes.size()) return nullptr;
    return nodes[static_cast<size_t>(index)].get();
}

accessibility::AndroidMapping AndroidAccessibility_MappingFor(const accessibility::AccessibilityNode* node) {
    if (!node) return accessibility::AndroidMapping{};
    return accessibility::MapRoleToAndroid(node->Role());
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang

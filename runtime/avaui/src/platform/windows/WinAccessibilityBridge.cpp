#include "WinAccessibilityBridge.h"

#include "components/ComponentTree.h"
#include "controls/ScrollView.h"
#include "events/Event.h"
#include "layout/LayoutEngine.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

namespace {

accessibility::AccessibilityTree* g_tree = nullptr;
events::IEventDispatcher* g_dispatcher = nullptr;
ComponentTree* g_componentTree = nullptr;
LayoutEngine* g_layoutEngine = nullptr;

double ApplyScrollAmount(double offset, double maxOffset, double viewportExtent, int amount) {
    if (amount == 0 || maxOffset <= 0.0) return offset;
    const double step = (amount == 2 || amount == -2) ? viewportExtent * 0.9 : viewportExtent * 0.1;
    const double delta = amount > 0 ? step : -step;
    return std::clamp(offset + delta, 0.0, maxOffset);
}

}

void WinAccessibility_SetTree(accessibility::AccessibilityTree* tree) {
    g_tree = tree;
}

void WinAccessibility_SetDispatcher(events::IEventDispatcher* dispatcher) {
    g_dispatcher = dispatcher;
}

void WinAccessibility_SetComponentTree(ComponentTree* componentTree) {
    g_componentTree = componentTree;
}

void WinAccessibility_SetLayoutEngine(LayoutEngine* layoutEngine) {
    g_layoutEngine = layoutEngine;
}

accessibility::AccessibilityNode* WinAccessibility_Root() {
    if (!g_tree) return nullptr;
    return g_tree->Root();
}

int WinAccessibility_NodeCount() {
    if (!g_tree) return 0;
    return static_cast<int>(g_tree->Nodes().size());
}

accessibility::AccessibilityNode* WinAccessibility_NodeAt(int index) {
    if (!g_tree) return nullptr;
    const auto& nodes = g_tree->Nodes();
    if (index < 0 || static_cast<size_t>(index) >= nodes.size()) return nullptr;
    return nodes[static_cast<size_t>(index)].get();
}

accessibility::AccessibilityNode* WinAccessibility_NodeById(ComponentId id) {
    if (!g_tree) return nullptr;
    return g_tree->FindById(id);
}

accessibility::WindowsMapping WinAccessibility_MappingFor(const accessibility::AccessibilityNode* node) {
    if (!node) return accessibility::WindowsMapping{};
    return accessibility::MapRoleToWindows(node->Role());
}

void WinAccessibility_Invoke(ComponentId id) {
    if (!g_dispatcher) return;
    events::Event clickEvent(events::EventType::Click, id);
    g_dispatcher->Dispatch(&clickEvent);
}

WinAccessibilityScrollInfo WinAccessibility_GetScrollInfo(ComponentId id) {
    WinAccessibilityScrollInfo info;
    if (!g_componentTree || !g_layoutEngine) return info;

    IComponent* component = g_componentTree->FindById(id);
    if (!component) return info;

    const double viewportX = controls::GetViewportExtent(component, g_layoutEngine, true);
    const double viewportY = controls::GetViewportExtent(component, g_layoutEngine, false);
    const double contentX = controls::GetContentExtent(component, g_layoutEngine, true);
    const double contentY = controls::GetContentExtent(component, g_layoutEngine, false);
    const double maxX = std::max(0.0, contentX - viewportX);
    const double maxY = std::max(0.0, contentY - viewportY);

    info.horizontallyScrollable = maxX > 0.0;
    info.verticallyScrollable = maxY > 0.0;
    info.horizontalViewSize = contentX > 0.0 ? std::min(100.0, viewportX / contentX * 100.0) : 100.0;
    info.verticalViewSize = contentY > 0.0 ? std::min(100.0, viewportY / contentY * 100.0) : 100.0;

    if (info.horizontallyScrollable) {
        info.horizontalScrollPercent = controls::GetScrollOffsetX(component) / maxX * 100.0;
    }
    if (info.verticallyScrollable) {
        info.verticalScrollPercent = controls::GetScrollOffsetY(component) / maxY * 100.0;
    }

    return info;
}

void WinAccessibility_SetScrollPercent(ComponentId id, double horizontalPercent, double verticalPercent) {
    if (!g_componentTree || !g_layoutEngine) return;

    IComponent* component = g_componentTree->FindById(id);
    if (!component) return;

    double offsetX = controls::GetScrollOffsetX(component);
    double offsetY = controls::GetScrollOffsetY(component);

    const double maxX = controls::GetMaxScrollOffset(component, g_layoutEngine, true);
    const double maxY = controls::GetMaxScrollOffset(component, g_layoutEngine, false);

    if (horizontalPercent >= 0.0) {
        offsetX = std::clamp(horizontalPercent, 0.0, 100.0) / 100.0 * maxX;
    }
    if (verticalPercent >= 0.0) {
        offsetY = std::clamp(verticalPercent, 0.0, 100.0) / 100.0 * maxY;
    }

    controls::SetScrollOffset(component, offsetX, offsetY);
}

void WinAccessibility_ScrollBy(ComponentId id, int horizontalAmount, int verticalAmount) {
    if (!g_componentTree || !g_layoutEngine) return;

    IComponent* component = g_componentTree->FindById(id);
    if (!component) return;

    const double maxX = controls::GetMaxScrollOffset(component, g_layoutEngine, true);
    const double maxY = controls::GetMaxScrollOffset(component, g_layoutEngine, false);
    const double viewportX = controls::GetViewportExtent(component, g_layoutEngine, true);
    const double viewportY = controls::GetViewportExtent(component, g_layoutEngine, false);

    const double offsetX = ApplyScrollAmount(controls::GetScrollOffsetX(component), maxX, viewportX, horizontalAmount);
    const double offsetY = ApplyScrollAmount(controls::GetScrollOffsetY(component), maxY, viewportY, verticalAmount);

    controls::SetScrollOffset(component, offsetX, offsetY);
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

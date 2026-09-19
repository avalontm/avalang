#ifndef AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYBRIDGE_H
#define AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYBRIDGE_H

#include "accessibility/AccessibilityTree.h"
#include "accessibility/AccessibilityMappings.h"
#include "events/IEventDispatcher.h"
#include "Fwd.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

struct WinAccessibilityScrollInfo {
    double horizontalScrollPercent = -1.0;
    double verticalScrollPercent = -1.0;
    double horizontalViewSize = 100.0;
    double verticalViewSize = 100.0;
    bool horizontallyScrollable = false;
    bool verticallyScrollable = false;
};

void WinAccessibility_SetTree(accessibility::AccessibilityTree* tree);
void WinAccessibility_SetDispatcher(events::IEventDispatcher* dispatcher);
void WinAccessibility_SetComponentTree(ComponentTree* componentTree);
void WinAccessibility_SetLayoutEngine(LayoutEngine* layoutEngine);

accessibility::AccessibilityNode* WinAccessibility_Root();
int WinAccessibility_NodeCount();
accessibility::AccessibilityNode* WinAccessibility_NodeAt(int index);
accessibility::AccessibilityNode* WinAccessibility_NodeById(ComponentId id);
accessibility::WindowsMapping WinAccessibility_MappingFor(const accessibility::AccessibilityNode* node);
void WinAccessibility_Invoke(ComponentId id);

WinAccessibilityScrollInfo WinAccessibility_GetScrollInfo(ComponentId id);
void WinAccessibility_SetScrollPercent(ComponentId id, double horizontalPercent, double verticalPercent);
void WinAccessibility_ScrollBy(ComponentId id, int horizontalAmount, int verticalAmount);

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYBRIDGE_H

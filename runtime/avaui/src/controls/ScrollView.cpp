#include "controls/ScrollView.h"
#include "components/PropertyValue.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "registry/ComponentTypeRegistry.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace controls {

double GetScrollOffsetX(IComponent* scrollViewComponent) {
    if (!scrollViewComponent) {
        return 0.0;
    }
    const auto* prop = scrollViewComponent->GetProperty("scrollOffsetX");
    if (!prop || prop->Type() != PropertyType::Number) {
        return 0.0;
    }
    return prop->AsNumber();
}

double GetScrollOffsetY(IComponent* scrollViewComponent) {
    if (!scrollViewComponent) {
        return 0.0;
    }
    const auto* prop = scrollViewComponent->GetProperty("scrollOffsetY");
    if (!prop || prop->Type() != PropertyType::Number) {
        return 0.0;
    }
    return prop->AsNumber();
}

void SetScrollOffset(IComponent* scrollViewComponent, double offsetX, double offsetY) {
    if (!scrollViewComponent) {
        return;
    }
    const InvalidationFlag scrollFlags = InvalidationFlag::Paint | InvalidationFlag::Scene;
    scrollViewComponent->SetProperty("scrollOffsetX", PropertyValue(offsetX), scrollFlags);
    scrollViewComponent->SetProperty("scrollOffsetY", PropertyValue(offsetY), scrollFlags);
}

bool IsHorizontalScrollView(IComponent* scrollViewComponent) {
    if (!scrollViewComponent) {
        return false;
    }
    const auto* direction = scrollViewComponent->GetProperty("direction");
    if (!direction || direction->Type() != PropertyType::String) {
        return false;
    }
    return direction->AsString() == "horizontal";
}

double GetViewportExtent(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal) {
    if (!scrollViewComponent || !layoutEngine) {
        return 0.0;
    }
    ILayoutNode* scrollNode = layoutEngine->FindNode(scrollViewComponent->Id());
    if (!scrollNode) {
        return 0.0;
    }
    const LayoutRect& viewportRect = scrollNode->Rect();
    return horizontal ? viewportRect.width : viewportRect.height;
}

double GetContentExtent(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal) {
    if (!scrollViewComponent || !layoutEngine) {
        return 0.0;
    }
    ILayoutNode* scrollNode = layoutEngine->FindNode(scrollViewComponent->Id());
    if (!scrollNode) {
        return 0.0;
    }
    const LayoutRect& viewportRect = scrollNode->Rect();
    double maxEdge = 0.0;
    for (ILayoutNode* child : scrollNode->Children()) {
        if (!child) continue;
        const LayoutRect& childRect = child->Rect();
        double edge = horizontal
            ? (childRect.x + childRect.width - viewportRect.x)
            : (childRect.y + childRect.height - viewportRect.y);
        maxEdge = std::max(maxEdge, edge);
    }
    return maxEdge;
}

double GetMaxScrollOffset(IComponent* scrollViewComponent, LayoutEngine* layoutEngine, bool horizontal) {
    const double viewportExtent = GetViewportExtent(scrollViewComponent, layoutEngine, horizontal);
    const double contentExtent = GetContentExtent(scrollViewComponent, layoutEngine, horizontal);
    return std::max(0.0, contentExtent - viewportExtent);
}

namespace {
struct ScrollViewTypeRegistrations {
    ScrollViewTypeRegistrations() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "ScrollView", "Scroll View", true,
            {
                {"scrollOffsetX", PropertyValue(0.0)},
                {"scrollOffsetY", PropertyValue(0.0)},
            },
        });
        RegisterComponentType({
            "ListView", "List View", true,
            {
                {"scrollOffsetX", PropertyValue(0.0)},
                {"scrollOffsetY", PropertyValue(0.0)},
            },
        });
    }
};
static ScrollViewTypeRegistrations _scrollview_type_registrations;
}

}
}
}

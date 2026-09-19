#include "controls/ScrollViewController.h"

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "controls/ScrollView.h"
#include "events/Event.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"

#include <algorithm>
#include <cmath>

namespace avalang {
namespace ui {
namespace controls {

namespace {

bool IsScrollViewType(const IComponent* node) {
    return node && (node->TypeName() == "ScrollView" || node->TypeName() == "ListView");
}

}

class ScrollViewController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* scrollView, LayoutEngine& layoutEngine)
        : scrollView_(scrollView), layoutEngine_(layoutEngine) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || !scrollView_) return;
        if (event->Type() != events::EventType::Wheel) return;

        auto* wheelEvent = dynamic_cast<events::IWheelEvent*>(event);
        if (!wheelEvent) return;

        ILayoutNode* scrollNode = layoutEngine_.FindNode(scrollView_->Id());
        if (!scrollNode) return;

        const bool horizontal = IsHorizontalScrollView(scrollView_);
        const double maxOffset = GetMaxScrollOffset(scrollView_, &layoutEngine_, horizontal);

        double offsetX = GetScrollOffsetX(scrollView_);
        double offsetY = GetScrollOffsetY(scrollView_);

        if (horizontal) {
            const double delta = wheelEvent->DeltaX() != 0.0f
                ? static_cast<double>(wheelEvent->DeltaX())
                : static_cast<double>(wheelEvent->DeltaY());
            offsetX = std::clamp(offsetX + delta, 0.0, maxOffset);
        } else {
            offsetY = std::clamp(offsetY + static_cast<double>(wheelEvent->DeltaY()), 0.0, maxOffset);
        }

        SetScrollOffset(scrollView_, offsetX, offsetY);
        event->StopPropagation();
    }

private:
    IComponent* scrollView_;
    LayoutEngine& layoutEngine_;
};

ScrollViewController::ScrollViewController(events::IEventDispatcher& dispatcher, LayoutEngine& layoutEngine)
    : dispatcher_(dispatcher), layoutEngine_(layoutEngine) {}

ScrollViewController::~ScrollViewController() = default;

void ScrollViewController::Attach(IComponent* root) {
    AttachRecursive(root, nullptr);
}

void ScrollViewController::AttachRecursive(IComponent* node, IComponent* currentScrollView) {
    if (!node) return;

    IComponent* effectiveScrollView = currentScrollView;

    if (IsScrollViewType(node)) {
        effectiveScrollView = node;
        auto handler = std::make_unique<Handler>(effectiveScrollView, layoutEngine_);
        dispatcher_.Subscribe(node->Id(), events::EventType::Wheel, handler.get());
        handlers_.push_back(std::move(handler));
    } else if (effectiveScrollView) {
        auto handler = std::make_unique<Handler>(effectiveScrollView, layoutEngine_);
        dispatcher_.Subscribe(node->Id(), events::EventType::Wheel, handler.get());
        handlers_.push_back(std::move(handler));
    }

    for (IComponent* child : node->Children()) {
        AttachRecursive(child, effectiveScrollView);
    }
}

}
}
}

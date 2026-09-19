#include "controls/ButtonController.h"

#include "components/IComponent.h"
#include "controls/ActivationKey.h"
#include "events/Event.h"

namespace avalang {
namespace ui {
namespace controls {

class ButtonController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, events::IEventDispatcher& dispatcher)
        : target_(target), dispatcher_(dispatcher) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || !target_) return;

        switch (event->Type()) {
            case events::EventType::KeyDown:
                HandleKeyDown(dynamic_cast<events::IKeyboardEvent*>(event));
                break;
            case events::EventType::PointerDown:
                pressed_ = true;
                break;
            case events::EventType::PointerUp:
                pressed_ = false;
                break;
            case events::EventType::Focus:
                focused_ = true;
                break;
            case events::EventType::Blur:
                focused_ = false;
                break;
            case events::EventType::Click:
                break;
            default:
                break;
        }
    }

    bool IsPressed() const { return pressed_; }
    bool IsFocused() const { return focused_; }

private:
    void HandleKeyDown(events::IKeyboardEvent* keyEvent) {
        if (!keyEvent) return;
        if (!IsActivationKey(keyEvent->TranslatedKey())) return;

        events::Event clickEvent(events::EventType::Click, target_->Id());
        dispatcher_.Dispatch(&clickEvent);
    }

    IComponent* target_;
    events::IEventDispatcher& dispatcher_;
    bool pressed_ = false;
    bool focused_ = false;
};

ButtonController::ButtonController(events::IEventDispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

ButtonController::~ButtonController() = default;

void ButtonController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void ButtonController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "Button") {
        auto handler = std::make_unique<Handler>(node, dispatcher_);
        dispatcher_.Subscribe(node->Id(), events::EventType::KeyDown, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::PointerDown, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::PointerUp, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::Click, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::Focus, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::Blur, handler.get());
        handlers_.push_back(std::move(handler));
    }

    for (IComponent* child : node->Children()) {
        AttachRecursive(child);
    }
}

}
}
}

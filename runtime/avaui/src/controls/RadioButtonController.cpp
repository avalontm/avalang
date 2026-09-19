#include "controls/RadioButtonController.h"

#include "components/IComponent.h"
#include "controls/ActivationKey.h"
#include "controls/RadioButton.h"
#include "events/Event.h"

namespace avalang {
namespace ui {
namespace controls {

class RadioButtonController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, events::IEventDispatcher& dispatcher)
        : target_(target), dispatcher_(dispatcher) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || !target_) return;

        switch (event->Type()) {
            case events::EventType::Click:
                Activate();
                break;
            case events::EventType::KeyDown:
                HandleKeyDown(dynamic_cast<events::IKeyboardEvent*>(event));
                break;
            default:
                break;
        }
    }

private:
    void HandleKeyDown(events::IKeyboardEvent* keyEvent) {
        if (!keyEvent) return;
        if (!IsActivationKey(keyEvent->TranslatedKey())) return;

        Activate();
    }

    void Activate() {
        if (GetRadioButtonSelected(target_)) return;

        SelectRadioButton(target_);

        events::Event changeEvent(events::EventType::Change, target_->Id());
        dispatcher_.Dispatch(&changeEvent);
    }

    IComponent* target_;
    events::IEventDispatcher& dispatcher_;
};

RadioButtonController::RadioButtonController(events::IEventDispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

RadioButtonController::~RadioButtonController() = default;

void RadioButtonController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void RadioButtonController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "RadioButton") {
        auto handler = std::make_unique<Handler>(node, dispatcher_);
        dispatcher_.Subscribe(node->Id(), events::EventType::Click, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::KeyDown, handler.get());
        handlers_.push_back(std::move(handler));
    }

    for (IComponent* child : node->Children()) {
        AttachRecursive(child);
    }
}

}
}
}

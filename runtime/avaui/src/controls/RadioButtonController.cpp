#include "controls/RadioButtonController.h"

#include "components/IComponent.h"
#include "controls/ActivationKey.h"
#include "controls/RadioButton.h"
#include "events/Event.h"

#include <utility>

namespace avalang {
namespace ui {
namespace controls {

class RadioButtonController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, events::IEventDispatcher& dispatcher, const RadioButtonBinding& binding)
        : target_(target), dispatcher_(dispatcher), binding_(binding) {}

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
        // Read from what is on screen, not from the raw property: for a
        // bound RadioButton the raw property is the text "planBasic", not a
        // bool (same reasoning as CheckBoxController).
        const bool alreadySelected = binding_.resolveSelected ? binding_.resolveSelected(target_)
                                                                : GetRadioButtonSelected(target_);
        if (alreadySelected) return;

        const bool committed = binding_.commitSelected && binding_.commitSelected(target_);
        if (!committed) {
            SelectRadioButton(target_);
        }

        events::Event changeEvent(events::EventType::Change, target_->Id());
        dispatcher_.Dispatch(&changeEvent);
    }

    IComponent* target_;
    events::IEventDispatcher& dispatcher_;
    const RadioButtonBinding& binding_;
};

RadioButtonController::RadioButtonController(events::IEventDispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

RadioButtonController::~RadioButtonController() = default;

void RadioButtonController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void RadioButtonController::SetBinding(RadioButtonBinding binding) {
    binding_ = std::move(binding);
}

void RadioButtonController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "RadioButton") {
        auto handler = std::make_unique<Handler>(node, dispatcher_, binding_);
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

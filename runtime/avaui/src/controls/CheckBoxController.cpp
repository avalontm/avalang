#include "controls/CheckBoxController.h"

#include "components/IComponent.h"
#include "controls/ActivationKey.h"
#include "controls/CheckBox.h"
#include "events/Event.h"

#include <utility>

namespace avalang {
namespace ui {
namespace controls {

class CheckBoxController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, events::IEventDispatcher& dispatcher, const CheckBoxBinding& binding)
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
        // Toggle from what is on screen, not from the raw property: for a
        // bound checkbox the raw property is the text "agreed", not a bool.
        const bool current = binding_.resolveChecked ? binding_.resolveChecked(target_)
                                                     : GetCheckBoxChecked(target_);
        const bool next = !current;

        const bool committed = binding_.commitChecked && binding_.commitChecked(target_, next);
        if (!committed) {
            SetCheckBoxChecked(target_, next);
        }

        events::Event changeEvent(events::EventType::Change, target_->Id());
        dispatcher_.Dispatch(&changeEvent);
    }

    IComponent* target_;
    events::IEventDispatcher& dispatcher_;
    const CheckBoxBinding& binding_;
};

CheckBoxController::CheckBoxController(events::IEventDispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

CheckBoxController::~CheckBoxController() = default;

void CheckBoxController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void CheckBoxController::SetBinding(CheckBoxBinding binding) {
    binding_ = std::move(binding);
}

void CheckBoxController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "CheckBox") {
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

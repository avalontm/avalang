#include "controls/ComboBoxController.h"

#include "components/IComponent.h"
#include "controls/ComboBox.h"
#include "events/Event.h"

namespace avalang {
namespace ui {
namespace controls {

namespace {

int SelectedIndex(IComponent* target) {
    const std::string selected = GetSelectedValue(target);
    const auto& children = target->Children();
    for (size_t i = 0; i < children.size(); ++i) {
        const auto* valueProp = children[i]->GetProperty("value");
        if (valueProp && valueProp->Type() == PropertyType::String &&
            valueProp->AsString() == selected) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MoveSelection(IComponent* target, events::IEventDispatcher& dispatcher, int direction) {
    const auto& children = target->Children();
    if (children.empty()) return;

    int index = SelectedIndex(target);
    int nextIndex = index < 0 ? 0 : index + direction;
    if (nextIndex < 0) nextIndex = 0;
    if (nextIndex >= static_cast<int>(children.size())) nextIndex = static_cast<int>(children.size()) - 1;
    if (nextIndex == index) return;

    const auto* valueProp = children[nextIndex]->GetProperty("value");
    const std::string nextValue =
        (valueProp && valueProp->Type() == PropertyType::String) ? valueProp->AsString() : "";

    SetSelectedValue(target, nextValue);

    events::Event changeEvent(events::EventType::Change, target->Id());
    dispatcher.Dispatch(&changeEvent);
}

}

class ComboBoxController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, events::IEventDispatcher& dispatcher)
        : target_(target), dispatcher_(dispatcher) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || !target_) return;

        switch (event->Type()) {
            case events::EventType::Click:
                SetComboBoxOpen(target_, !IsComboBoxOpen(target_));
                break;
            case events::EventType::Blur:
                if (IsComboBoxOpen(target_)) {
                    SetComboBoxOpen(target_, false);
                }
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

        switch (keyEvent->TranslatedKey()) {
            case events::Key::Escape:
                if (IsComboBoxOpen(target_)) {
                    SetComboBoxOpen(target_, false);
                }
                break;
            case events::Key::Enter:
            case events::Key::NumpadEnter:
                SetComboBoxOpen(target_, !IsComboBoxOpen(target_));
                break;
            case events::Key::ArrowDown:
                MoveSelection(target_, dispatcher_, 1);
                break;
            case events::Key::ArrowUp:
                MoveSelection(target_, dispatcher_, -1);
                break;
            default:
                break;
        }
    }

    IComponent* target_;
    events::IEventDispatcher& dispatcher_;
};

ComboBoxController::ComboBoxController(events::IEventDispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

ComboBoxController::~ComboBoxController() = default;

void ComboBoxController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void ComboBoxController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "ComboBox") {
        auto handler = std::make_unique<Handler>(node, dispatcher_);
        dispatcher_.Subscribe(node->Id(), events::EventType::Click, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::Blur, handler.get());
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

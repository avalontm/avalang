#include "controls/TextBoxEditingController.h"

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "controls/TextBox.h"
#include "controls/TextEditBuffer.h"
#include "events/Key.h"

namespace avalang {
namespace ui {
namespace controls {

namespace {

int PropInt(IComponent* comp, const char* name, int fallback) {
    const PropertyValue* prop = comp->GetProperty(name);
    if (!prop || prop->Type() != PropertyType::Number) return fallback;
    return static_cast<int>(prop->AsNumber());
}

std::string StripControlChars(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7F) continue;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

TextEditState LoadState(IComponent* comp) {
    TextEditState state;
    state.text = GetTextBoxValue(comp);
    state.caret = TextEditClampBoundary(state.text, PropInt(comp, "caretIndex", static_cast<int>(state.text.size())));
    state.anchor = TextEditClampBoundary(state.text, PropInt(comp, "selectionAnchor", state.caret));
    return state;
}

void StoreCaret(IComponent* comp, const TextEditState& state) {
    comp->SetProperty("caretIndex", PropertyValue(static_cast<double>(state.caret)));
    comp->SetProperty("selectionAnchor", PropertyValue(static_cast<double>(state.anchor)));
}

void CommitState(IComponent* comp, const TextEditState& state, bool textChanged) {
    StoreCaret(comp, state);
    if (textChanged) {
        SetTextBoxValue(comp, state.text);
    }
}

}

class TextBoxEditingController::Handler final : public events::IEventHandler {
public:
    Handler(IComponent* target, ava::platform::ui::IClipboard& clipboard)
        : target_(target), clipboard_(clipboard) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || !target_) return;

        switch (event->Type()) {
            case events::EventType::Focus:
                HandleFocus();
                break;
            case events::EventType::KeyDown:
                HandleKeyDown(dynamic_cast<events::IKeyboardEvent*>(event));
                break;
            case events::EventType::TextInput:
                HandleTextInput(dynamic_cast<events::ITextInputEvent*>(event));
                break;
            case events::EventType::Ime:
                HandleIme(dynamic_cast<events::IImeEvent*>(event));
                break;
            default:
                break;
        }
    }

private:
    void HandleFocus() {
        TextEditState state = LoadState(target_);
        state.caret = static_cast<int>(state.text.size());
        state.anchor = state.caret;
        StoreCaret(target_, state);
    }

    void HandleKeyDown(events::IKeyboardEvent* keyEvent) {
        if (!keyEvent) return;

        TextEditState state = LoadState(target_);
        bool textChanged = false;
        const bool ctrl = keyEvent->IsCtrlDown();
        const bool shift = keyEvent->IsShiftDown();

        switch (keyEvent->TranslatedKey()) {
            case events::Key::Backspace:
                state = TextEditBackspace(state);
                textChanged = true;
                break;
            case events::Key::Delete:
                state = TextEditDeleteForward(state);
                textChanged = true;
                break;
            case events::Key::ArrowLeft:
                state = TextEditMoveLeft(state, shift);
                break;
            case events::Key::ArrowRight:
                state = TextEditMoveRight(state, shift);
                break;
            case events::Key::Home:
                state = TextEditMoveHome(state, shift);
                break;
            case events::Key::End:
                state = TextEditMoveEnd(state, shift);
                break;
            case events::Key::A:
                if (!ctrl) return;
                state = TextEditSelectAll(state);
                break;
            case events::Key::C:
                if (!ctrl || !TextEditHasSelection(state)) return;
                clipboard_.SetText(TextEditSelectedText(state));
                return;
            case events::Key::X:
                if (!ctrl || !TextEditHasSelection(state)) return;
                clipboard_.SetText(TextEditSelectedText(state));
                state = TextEditDeleteSelection(state);
                textChanged = true;
                break;
            case events::Key::V:
                if (!ctrl || !clipboard_.HasText()) return;
                state = TextEditInsert(state, StripControlChars(clipboard_.GetText()));
                textChanged = true;
                break;
            default:
                return;
        }

        CommitState(target_, state, textChanged);
    }

    void HandleTextInput(events::ITextInputEvent* textEvent) {
        if (!textEvent) return;

        const std::string committed = StripControlChars(textEvent->Text());
        if (committed.empty()) return;

        TextEditState state = LoadState(target_);
        state = TextEditInsert(state, committed);
        CommitState(target_, state, true);
    }

    void HandleIme(events::IImeEvent* imeEvent) {
        if (!imeEvent) return;

        if (imeEvent->IsComposing()) {
            target_->SetProperty("imeComposition", PropertyValue(imeEvent->CompositionText()));
            target_->SetProperty("imeCompositionCursor",
                                  PropertyValue(static_cast<double>(imeEvent->CompositionCursor())));
        } else {
            target_->SetProperty("imeComposition", PropertyValue(std::string()));
            target_->SetProperty("imeCompositionCursor", PropertyValue(0.0));
        }
    }

    IComponent* target_;
    ava::platform::ui::IClipboard& clipboard_;
};

TextBoxEditingController::TextBoxEditingController(events::IEventDispatcher& dispatcher,
                                                     ava::platform::ui::IClipboard& clipboard)
    : dispatcher_(dispatcher), clipboard_(clipboard) {}

TextBoxEditingController::~TextBoxEditingController() = default;

void TextBoxEditingController::Attach(IComponent* root) {
    AttachRecursive(root);
}

void TextBoxEditingController::AttachRecursive(IComponent* node) {
    if (!node) return;

    if (node->TypeName() == "TextBox") {
        auto handler = std::make_unique<Handler>(node, clipboard_);
        dispatcher_.Subscribe(node->Id(), events::EventType::Focus, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::KeyDown, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::TextInput, handler.get());
        dispatcher_.Subscribe(node->Id(), events::EventType::Ime, handler.get());
        handlers_.push_back(std::move(handler));
    }

    for (IComponent* child : node->Children()) {
        AttachRecursive(child);
    }
}

}
}
}

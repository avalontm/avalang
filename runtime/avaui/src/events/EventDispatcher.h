#ifndef AVA_UI_EVENTS_EVENT_DISPATCHER_H
#define AVA_UI_EVENTS_EVENT_DISPATCHER_H

#include "events/IEventDispatcher.h"
#include "layout/ILayoutNode.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace ava {
namespace platform {
namespace ui {
class IMouse;
class IKeyboard;
class IWindow;
} // namespace ui
} // namespace platform
} // namespace ava

namespace avalang {
namespace ui {

class LayoutEngine;

namespace events {

struct EventSubscription {
    EventHandlerId id;
    ComponentId target;
    EventType type;
    IEventHandler* handler;  // non-owning
};

class EventDispatcher : public IEventDispatcher {
public:
    EventDispatcher();
    ~EventDispatcher();

    EventHandlerId Subscribe(ComponentId target, EventType type, IEventHandler* handler) override;
    void Unsubscribe(EventHandlerId id) override;

    void Dispatch(IEvent* event) override;
    void PollInput(IComponent* root) override;

    ComponentId FocusedComponent() const override { return focusedComponent_; }
    void SetFocusedComponent(ComponentId id) override;

    int MouseX() const override { return mouseX_; }
    int MouseY() const override { return mouseY_; }
    bool IsMouseButtonDown(MouseButton btn) const override;

    // Allow layout engine injection for hit-testing
    void SetLayoutEngine(LayoutEngine* engine) { layoutEngine_ = engine; }

    // Allow platform input injection (Phase 11: Native Backend). Both are
    // non-owning; caller (e.g. NativeWindowHost) keeps them alive at
    // least as long as this dispatcher.
    void SetPlatformInput(ava::platform::ui::IMouse* mouse, ava::platform::ui::IKeyboard* keyboard) {
        platformMouse_ = mouse;
        platformKeyboard_ = keyboard;
    }

private:
    void DispatchWithBubbling(IEvent* event, IComponent* root);
    ComponentId HitTest(IComponent* root, int x, int y);
    ComponentId HitTestRecursive(IComponent* node, const ILayoutNode* layoutNode, int x, int y);

    std::vector<EventSubscription> subscriptions_;
    EventHandlerId nextHandlerId_ = 1;

    ava::platform::ui::IMouse* platformMouse_ = nullptr;
    ava::platform::ui::IKeyboard* platformKeyboard_ = nullptr;
    LayoutEngine* layoutEngine_ = nullptr;

    ComponentId focusedComponent_ = 0;
    int mouseX_ = 0, mouseY_ = 0;
    bool mouseButtonState_[3] = {false, false, false};  // L, R, M
    int prevMouseX_ = 0, prevMouseY_ = 0;
    bool prevMouseButtonState_[3] = {false, false, false};

    // Fase 20.0 (freeze plan, "Finish Event Dispatcher"): target hit by
    // the most recent Left MouseDown, kept only to detect a classic
    // click (down and up landing on the same component) in PollInput.
    // EventType::Click has existed in IEvent.h's enum since Phase 5 but
    // nothing ever synthesized one -- see PollInput's left-button-up
    // branch. 0 (no component) means "no button currently pressed" or
    // "press/release targets diverged", both of which suppress Click.
    ComponentId mouseDownTarget_ = 0;

    ComponentId hoveredTarget_ = 0;

    std::unordered_map<int, bool> keyboardState_;  // keyCode -> isDown
    std::unordered_map<int, bool> prevKeyboardState_;
};

} // namespace events
} // namespace ui
} // namespace avalang

#endif // AVA_UI_EVENTS_EVENT_DISPATCHER_H

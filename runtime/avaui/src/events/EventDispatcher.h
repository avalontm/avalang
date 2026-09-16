#pragma once

#include "Export.h"
#include "events/IEventDispatcher.h"
#include "events/Key.h"
#include "layout/ILayoutNode.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace ava {
namespace platform {
namespace ui {
class IMouse;
class IKeyboard;
class IWheel;
class ITextInput;
class ITouch;
class IIme;
}
}
}

namespace avalang {
namespace ui {

class LayoutEngine;

namespace events {

struct EventSubscription {
    EventHandlerId id;
    ComponentId target;
    EventType type;
    IEventHandler* handler;
};

struct TrackedTouch {
    ComponentId target = 0;
    int startX = 0;
    int startY = 0;
    int x = 0;
    int y = 0;
    uint64_t startTime = 0;
    bool longPressFired = false;
};

class AVA_UI_API EventDispatcher : public IEventDispatcher {
public:
    EventDispatcher();
    ~EventDispatcher();

    EventHandlerId Subscribe(ComponentId target, EventType type, IEventHandler* handler) override;
    void Unsubscribe(EventHandlerId id) override;

    void Dispatch(IEvent* event) override;
    void PollInput(IComponent* root) override;

    ComponentId FocusedComponent() const override { return focusedComponent_; }
    void SetFocusedComponent(ComponentId id) override;

    int PointerX() const override { return pointerX_; }
    int PointerY() const override { return pointerY_; }
    bool IsPointerButtonDown(PointerButton button) const override;

    void SetLayoutEngine(LayoutEngine* engine) { layoutEngine_ = engine; }

    void SetPlatformInput(ava::platform::ui::IMouse* mouse, ava::platform::ui::IKeyboard* keyboard) {
        platformMouse_ = mouse;
        platformKeyboard_ = keyboard;
    }

    void SetKeyTranslator(NativeKeyTranslator translator) { keyTranslator_ = std::move(translator); }

    void SetWheelSource(ava::platform::ui::IWheel* wheel) { platformWheel_ = wheel; }
    void SetTextInputSource(ava::platform::ui::ITextInput* textInput) { platformTextInput_ = textInput; }
    void SetTouchSource(ava::platform::ui::ITouch* touch) { platformTouch_ = touch; }
    void SetImeSource(ava::platform::ui::IIme* ime) { platformIme_ = ime; }

    void SetTapMaxMovement(int pixels) { tapMaxMovement_ = pixels; }
    void SetLongPressMs(uint64_t ms) { longPressMs_ = ms; }
    void SetPanMinMovement(int pixels) { panMinMovement_ = pixels; }

private:
    void DispatchWithBubbling(IEvent* event, IComponent* root);
    ComponentId HitTest(IComponent* root, int x, int y);
    ComponentId HitTestRecursive(IComponent* node, const ILayoutNode* layoutNode, int x, int y);

    void PollWheel(IComponent* root);
    void PollTextInput();
    void PollIme();
    void PollTouch(IComponent* root);

    void BeginGestureTracking(ComponentId target, uint64_t id, int x, int y, uint64_t timestamp);
    void UpdateGestureTracking(uint64_t id, int x, int y, uint64_t timestamp, IComponent* root);
    void EndGestureTracking(uint64_t id, int x, int y, uint64_t timestamp, bool cancelled);

    std::vector<EventSubscription> subscriptions_;
    EventHandlerId nextHandlerId_ = 1;

    ava::platform::ui::IMouse* platformMouse_ = nullptr;
    ava::platform::ui::IKeyboard* platformKeyboard_ = nullptr;
    ava::platform::ui::IWheel* platformWheel_ = nullptr;
    ava::platform::ui::ITextInput* platformTextInput_ = nullptr;
    ava::platform::ui::ITouch* platformTouch_ = nullptr;
    ava::platform::ui::IIme* platformIme_ = nullptr;
    LayoutEngine* layoutEngine_ = nullptr;

    NativeKeyTranslator keyTranslator_;

    ComponentId focusedComponent_ = 0;
    int pointerX_ = 0, pointerY_ = 0;
    bool pointerButtonState_[3] = {false, false, false};
    int prevPointerX_ = 0, prevPointerY_ = 0;
    bool prevPointerButtonState_[3] = {false, false, false};

    ComponentId pointerDownTarget_ = 0;
    ComponentId hoveredTarget_ = 0;

    std::unordered_map<int, bool> keyboardState_;
    std::unordered_map<int, bool> prevKeyboardState_;

    std::unordered_map<uint64_t, TrackedTouch> activeTouches_;
    bool imeWasComposing_ = false;

    int tapMaxMovement_ = 8;
    uint64_t longPressMs_ = 500;
    int panMinMovement_ = 10;
};

}
}
}

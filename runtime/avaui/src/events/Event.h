#pragma once

#include "Export.h"
#include "events/IEvent.h"
#include <chrono>

namespace avalang {
namespace ui {
namespace events {

class AVA_UI_API Event : public virtual IEvent {
public:
    explicit Event(EventType type, ComponentId target);
    virtual ~Event() = default;

    EventType Type() const override { return type_; }
    ComponentId Target() const override { return target_; }
    void SetTarget(ComponentId id) override { target_ = id; }

    bool IsPropagating() const override { return propagating_; }
    void StopPropagation() override { propagating_ = false; }

    bool IsDefaultPrevented() const override { return preventedDefault_; }
    void PreventDefault() override { preventedDefault_ = true; }

    uint64_t Timestamp() const override { return timestamp_; }

private:
    EventType type_;
    ComponentId target_;
    bool propagating_ = true;
    bool preventedDefault_ = false;
    uint64_t timestamp_;
};

class AVA_UI_API PointerEvent : public Event, public IPointerEvent {
public:
    PointerEvent(EventType type, ComponentId target, PointerButton button, int x, int y);
    PointerEvent(EventType type, ComponentId target, int x, int y, int dx, int dy);

    PointerButton Button() const override { return button_; }
    int X() const override { return x_; }
    int Y() const override { return y_; }
    int DeltaX() const override { return deltaX_; }
    int DeltaY() const override { return deltaY_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    PointerButton button_;
    int x_, y_;
    int deltaX_ = 0, deltaY_ = 0;
};

class AVA_UI_API KeyboardEvent : public Event, public IKeyboardEvent {
public:
    KeyboardEvent(EventType type, ComponentId target, int keyCode, Key translatedKey = Key::Unknown,
                  bool shift = false, bool ctrl = false, bool alt = false, bool meta = false);

    int KeyCode() const override { return keyCode_; }
    Key TranslatedKey() const override { return translatedKey_; }
    bool IsShiftDown() const override { return shift_; }
    bool IsCtrlDown() const override { return ctrl_; }
    bool IsAltDown() const override { return alt_; }
    bool IsMetaDown() const override { return meta_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    int keyCode_;
    Key translatedKey_;
    bool shift_, ctrl_, alt_, meta_;
};

class AVA_UI_API WheelEvent : public Event, public IWheelEvent {
public:
    WheelEvent(ComponentId target, int x, int y, float deltaX, float deltaY);

    int X() const override { return x_; }
    int Y() const override { return y_; }
    float DeltaX() const override { return deltaX_; }
    float DeltaY() const override { return deltaY_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    int x_, y_;
    float deltaX_, deltaY_;
};

class AVA_UI_API TextInputEvent : public Event, public ITextInputEvent {
public:
    TextInputEvent(ComponentId target, std::string text);

    const std::string& Text() const override { return text_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    std::string text_;
};

class AVA_UI_API TouchEvent : public Event, public ITouchEvent {
public:
    TouchEvent(EventType type, ComponentId target, std::vector<TouchPoint> points);

    const std::vector<TouchPoint>& Points() const override { return points_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    std::vector<TouchPoint> points_;
};

class AVA_UI_API GestureEvent : public Event, public IGestureEvent {
public:
    GestureEvent(ComponentId target, GestureType gesture, int x, int y,
                 float translationX = 0.0f, float translationY = 0.0f, float scale = 1.0f);

    GestureType Gesture() const override { return gesture_; }
    int X() const override { return x_; }
    int Y() const override { return y_; }
    float TranslationX() const override { return translationX_; }
    float TranslationY() const override { return translationY_; }
    float Scale() const override { return scale_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    GestureType gesture_;
    int x_, y_;
    float translationX_, translationY_, scale_;
};

class AVA_UI_API ImeEvent : public Event, public IImeEvent {
public:
    ImeEvent(ComponentId target, std::string compositionText, int cursor, bool composing);

    const std::string& CompositionText() const override { return compositionText_; }
    int CompositionCursor() const override { return cursor_; }
    bool IsComposing() const override { return composing_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    std::string compositionText_;
    int cursor_;
    bool composing_;
};

class AVA_UI_API FocusEvent : public Event, public IFocusEvent {
public:
    FocusEvent(EventType type, ComponentId target, ComponentId relatedTarget);

    ComponentId RelatedTarget() const override { return relatedTarget_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    ComponentId relatedTarget_;
};

}
}
}

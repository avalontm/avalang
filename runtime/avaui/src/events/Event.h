#ifndef AVA_UI_EVENTS_EVENT_H
#define AVA_UI_EVENTS_EVENT_H

#include "events/IEvent.h"
#include <chrono>

namespace avalang {
namespace ui {
namespace events {

// Base event implementation
class Event : public virtual IEvent {
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

// Mouse event implementation
class MouseEvent : public Event, public IMouseEvent {
public:
    MouseEvent(EventType type, ComponentId target, MouseButton button, int x, int y);
    MouseEvent(EventType type, ComponentId target, int x, int y, int dx, int dy);  // scroll

    MouseButton Button() const override { return button_; }
    int X() const override { return x_; }
    int Y() const override { return y_; }
    int DeltaX() const override { return deltaX_; }
    int DeltaY() const override { return deltaY_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    MouseButton button_;
    int x_, y_;
    int deltaX_ = 0, deltaY_ = 0;
};

// Keyboard event implementation
class KeyboardEvent : public Event, public IKeyboardEvent {
public:
    KeyboardEvent(EventType type, ComponentId target, int keyCode,
                  bool shift = false, bool ctrl = false, bool alt = false, bool meta = false);

    int KeyCode() const override { return keyCode_; }
    bool IsShiftDown() const override { return shift_; }
    bool IsCtrlDown() const override { return ctrl_; }
    bool IsAltDown() const override { return alt_; }
    bool IsMetaDown() const override { return meta_; }

    IEvent* AsIEvent() { return static_cast<IEvent*>(this); }

private:
    int keyCode_;
    bool shift_, ctrl_, alt_, meta_;
};

} // namespace events
} // namespace ui
} // namespace avalang

#endif // AVA_UI_EVENTS_EVENT_H

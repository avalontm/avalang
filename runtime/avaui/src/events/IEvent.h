#ifndef AVA_UI_EVENTS_IEVENT_H
#define AVA_UI_EVENTS_IEVENT_H

#include "components/IComponent.h"
#include <cstdint>

namespace avalang {
namespace ui {
namespace events {

enum class EventType : uint8_t {
    // Mouse events
    MouseDown,
    MouseUp,
    MouseMove,
    MouseEnter,
    MouseLeave,
    Click,
    DoubleClick,
    Scroll,

    // Keyboard events
    KeyDown,
    KeyUp,
    KeyPress,

    // Focus events
    Focus,
    Blur,

    // Custom/system
    Custom,
};

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    None,
};

// Base event interface
class IEvent {
public:
    virtual ~IEvent() = default;

    virtual EventType Type() const = 0;
    virtual ComponentId Target() const = 0;
    virtual void SetTarget(ComponentId id) = 0;

    // Bubble/capture control
    virtual bool IsPropagating() const = 0;
    virtual void StopPropagation() = 0;

    virtual bool IsDefaultPrevented() const = 0;
    virtual void PreventDefault() = 0;

    // Timestamp (ms from start)
    virtual uint64_t Timestamp() const = 0;
};

// Mouse event
class IMouseEvent : public virtual IEvent {
public:
    virtual MouseButton Button() const = 0;
    virtual int X() const = 0;
    virtual int Y() const = 0;
    virtual int DeltaX() const = 0;  // for scroll
    virtual int DeltaY() const = 0;  // for scroll
};

// Keyboard event
class IKeyboardEvent : public virtual IEvent {
public:
    virtual int KeyCode() const = 0;
    virtual bool IsShiftDown() const = 0;
    virtual bool IsCtrlDown() const = 0;
    virtual bool IsAltDown() const = 0;
    virtual bool IsMetaDown() const = 0;
};

} // namespace events
} // namespace ui
} // namespace avalang

#endif // AVA_UI_EVENTS_IEVENT_H

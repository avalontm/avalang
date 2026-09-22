#pragma once

#include "components/IComponent.h"
#include "events/Key.h"
#include <cstdint>
#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace events {

enum class EventType : uint8_t {
    PointerDown,
    PointerUp,
    PointerMove,
    PointerEnter,
    PointerLeave,
    PointerCancel,
    Click,
    DoubleClick,
    Wheel,
    KeyDown,
    KeyUp,
    KeyPress,
    TextInput,
    TouchStart,
    TouchMove,
    TouchEnd,
    TouchCancel,
    Gesture,
    Ime,
    Focus,
    Blur,
    Change,
    Custom,
};

enum class PointerButton : uint8_t {
    Left,
    Right,
    Middle,
    None,
};

enum class TouchPhase : uint8_t {
    Started,
    Moved,
    Ended,
    Cancelled,
};

struct TouchPoint {
    uint64_t id = 0;
    int x = 0;
    int y = 0;
    TouchPhase phase = TouchPhase::Started;
};

enum class GestureType : uint8_t {
    Tap,
    DoubleTap,
    LongPress,
    Pan,
    Pinch,
    Swipe,
};

class IEvent {
public:
    virtual ~IEvent() = default;

    virtual EventType Type() const = 0;
    virtual ComponentId Target() const = 0;
    virtual void SetTarget(ComponentId id) = 0;

    virtual bool IsPropagating() const = 0;
    virtual void StopPropagation() = 0;

    virtual bool IsDefaultPrevented() const = 0;
    virtual void PreventDefault() = 0;

    virtual uint64_t Timestamp() const = 0;
};

class IPointerEvent : public virtual IEvent {
public:
    virtual PointerButton Button() const = 0;
    virtual int X() const = 0;
    virtual int Y() const = 0;
    virtual int DeltaX() const = 0;
    virtual int DeltaY() const = 0;
};

class IKeyboardEvent : public virtual IEvent {
public:
    virtual int KeyCode() const = 0;
    virtual Key TranslatedKey() const = 0;
    virtual bool IsShiftDown() const = 0;
    virtual bool IsCtrlDown() const = 0;
    virtual bool IsAltDown() const = 0;
    virtual bool IsMetaDown() const = 0;
};

class IWheelEvent : public virtual IEvent {
public:
    virtual int X() const = 0;
    virtual int Y() const = 0;
    virtual float DeltaX() const = 0;
    virtual float DeltaY() const = 0;
};

class ITextInputEvent : public virtual IEvent {
public:
    virtual const std::string& Text() const = 0;
};

class ITouchEvent : public virtual IEvent {
public:
    virtual const std::vector<TouchPoint>& Points() const = 0;
};

class IGestureEvent : public virtual IEvent {
public:
    virtual GestureType Gesture() const = 0;
    virtual int X() const = 0;
    virtual int Y() const = 0;
    virtual float TranslationX() const = 0;
    virtual float TranslationY() const = 0;
    virtual float Scale() const = 0;
};

class IImeEvent : public virtual IEvent {
public:
    virtual const std::string& CompositionText() const = 0;
    virtual int CompositionCursor() const = 0;
    virtual bool IsComposing() const = 0;
};

class IFocusEvent : public virtual IEvent {
public:
    virtual ComponentId RelatedTarget() const = 0;
};

class IChangeEvent : public virtual IEvent {
public:
    virtual const PropertyValue& Value() const = 0;
};

}
}
}

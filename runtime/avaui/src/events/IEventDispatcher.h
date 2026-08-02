#ifndef AVA_UI_EVENTS_IEVENT_DISPATCHER_H
#define AVA_UI_EVENTS_IEVENT_DISPATCHER_H

#include "events/IEvent.h"
#include "Export.h"
#include <cstddef>

namespace avalang {
namespace ui {
namespace events {

using EventHandlerId = size_t;

// Callback signature: (event) -> void
// Handler can call event->StopPropagation() or event->PreventDefault()
class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    virtual void OnEvent(IEvent* event) = 0;
};

// Centralized event dispatcher for a component tree.
// Owns event objects, coordinates polling/dispatch, manages bubbling.
class AVA_UI_API IEventDispatcher {
public:
    virtual ~IEventDispatcher() = default;

    // Factory
    static IEventDispatcher* Create();

    // Subscribe component to event type.
    // Returns handler ID for unsubscribe.
    virtual EventHandlerId Subscribe(ComponentId target, EventType type, IEventHandler* handler) = 0;
    virtual void Unsubscribe(EventHandlerId id) = 0;

    // Dispatch an event. Handles bubbling/capturing.
    // event->Target() determines initial component; bubbles up to root unless stopped.
    virtual void Dispatch(IEvent* event) = 0;

    // Poll input from PAL and emit events.
    // Call this once per frame before rendering.
    virtual void PollInput(IComponent* root) = 0;

    // Focus management
    virtual ComponentId FocusedComponent() const = 0;
    virtual void SetFocusedComponent(ComponentId id) = 0;

    // Mouse state cache (for hit-testing, tracking)
    virtual int MouseX() const = 0;
    virtual int MouseY() const = 0;
    virtual bool IsMouseButtonDown(MouseButton btn) const = 0;
};

} // namespace events
} // namespace ui
} // namespace avalang

#endif // AVA_UI_EVENTS_IEVENT_DISPATCHER_H

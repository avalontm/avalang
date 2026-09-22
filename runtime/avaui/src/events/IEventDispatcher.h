#pragma once

#include "events/IEvent.h"
#include "Export.h"
#include <cstddef>

namespace avalang {
namespace ui {
namespace events {

using EventHandlerId = size_t;

class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    virtual void OnEvent(IEvent* event) = 0;
};

class AVA_UI_API IEventDispatcher {
public:
    virtual ~IEventDispatcher() = default;

    static IEventDispatcher* Create();

    virtual EventHandlerId Subscribe(ComponentId target, EventType type, IEventHandler* handler) = 0;
    virtual void Unsubscribe(EventHandlerId id) = 0;

    virtual void Dispatch(IEvent* event) = 0;
    virtual void DispatchChange(ComponentId target, PropertyValue value) = 0;

    virtual void PollInput(IComponent* root) = 0;

    virtual ComponentId FocusedComponent() const = 0;
    virtual void SetFocusedComponent(ComponentId id) = 0;

    virtual int PointerX() const = 0;
    virtual int PointerY() const = 0;
    virtual bool IsPointerButtonDown(PointerButton button) const = 0;
};

}
}
}
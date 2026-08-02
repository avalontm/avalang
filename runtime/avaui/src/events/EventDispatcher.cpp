#include "events/EventDispatcher.h"
#include "events/Event.h"
#include "components/IComponent.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "../../../avalang/platform/interfaces/services/ui/IMouse.h"
#include "../../../avalang/platform/interfaces/services/ui/IKeyboard.h"
#include "../../../avalang/platform/interfaces/services/ui/IPlatformServices.h"
#include <algorithm>
#include <cstring>

namespace avalang {
namespace ui {
namespace events {

using namespace ava::platform::ui;

EventDispatcher::EventDispatcher() {
    // Phase 5 left this dispatcher without a real input source (there was
    // no PAL wiring to pull from yet -- see docs/AVALANG_UI_PROGRESS.md
    // Phase 5 limitations). platformMouse_/platformKeyboard_ start null;
    // Phase 11 (Native Backend) is what actually has a live IMouse/
    // IKeyboard to hand over, via SetPlatformInput() below.
}

EventDispatcher::~EventDispatcher() = default;

EventHandlerId EventDispatcher::Subscribe(ComponentId target, EventType type, IEventHandler* handler) {
    if (!handler) return 0;
    
    EventHandlerId id = nextHandlerId_++;
    subscriptions_.push_back({id, target, type, handler});
    return id;
}

void EventDispatcher::Unsubscribe(EventHandlerId id) {
    auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                          [id](const EventSubscription& s) { return s.id == id; });
    if (it != subscriptions_.end()) {
        subscriptions_.erase(it);
    }
}

void EventDispatcher::SetFocusedComponent(ComponentId id) {
    if (id == focusedComponent_) return;

    ComponentId previous = focusedComponent_;
    focusedComponent_ = id;

    if (previous != 0) {
        auto blur = std::make_unique<Event>(EventType::Blur, previous);
        Dispatch(blur.get());
    }
    if (id != 0) {
        auto focus = std::make_unique<Event>(EventType::Focus, id);
        Dispatch(focus.get());
    }
}

void EventDispatcher::Dispatch(IEvent* event) {
    if (!event) return;
    
    // Direct dispatch to target first
    for (const auto& sub : subscriptions_) {
        if (sub.target == event->Target() && sub.type == event->Type()) {
            sub.handler->OnEvent(event);
            if (!event->IsPropagating()) return;
        }
    }
}

void EventDispatcher::PollInput(IComponent* root) {
    if (!root || !platformMouse_ || !platformKeyboard_) return;

    // Save previous state
    prevMouseX_ = mouseX_;
    prevMouseY_ = mouseY_;
    std::memcpy(prevMouseButtonState_, mouseButtonState_, sizeof(mouseButtonState_));
    prevKeyboardState_ = keyboardState_;

    // Poll mouse
    int newX, newY;
    platformMouse_->Position(newX, newY);
    mouseX_ = newX;
    mouseY_ = newY;

    bool leftDown = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Left);
    bool rightDown = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Right);
    bool middleDown = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Middle);

    mouseButtonState_[0] = leftDown;
    mouseButtonState_[1] = rightDown;
    mouseButtonState_[2] = middleDown;

    ComponentId hitTarget = HitTest(root, mouseX_, mouseY_);

    // Mouse move event
    if (mouseX_ != prevMouseX_ || mouseY_ != prevMouseY_) {
        auto moveEvent = std::make_unique<MouseEvent>(EventType::MouseMove, hitTarget, MouseButton::None, mouseX_, mouseY_);
        Dispatch(moveEvent.get());
    }

    if (hitTarget != hoveredTarget_) {
        if (hoveredTarget_ != 0) {
            auto leaveEvent = std::make_unique<MouseEvent>(EventType::MouseLeave, hoveredTarget_, MouseButton::None, mouseX_, mouseY_);
            Dispatch(leaveEvent.get());
        }
        if (hitTarget != 0) {
            auto enterEvent = std::make_unique<MouseEvent>(EventType::MouseEnter, hitTarget, MouseButton::None, mouseX_, mouseY_);
            Dispatch(enterEvent.get());
        }
        hoveredTarget_ = hitTarget;
    }

    // Mouse down/up events
    if (leftDown != prevMouseButtonState_[0]) {
        ComponentId target = HitTest(root, mouseX_, mouseY_);
        EventType type = leftDown ? EventType::MouseDown : EventType::MouseUp;
        auto event = std::make_unique<MouseEvent>(type, target, MouseButton::Left, mouseX_, mouseY_);
        Dispatch(event.get());
        
        if (leftDown) {
            SetFocusedComponent(target);
            // Fase 20.0: remember where this press landed so the
            // matching release can tell a click from a drag-off.
            mouseDownTarget_ = target;
        } else {
            // Classic click: release lands on the same component the
            // press did (target != 0 excludes "released over empty
            // space"). See EventDispatcher.h's mouseDownTarget_ comment
            // for why this was never wired up before Fase 20.0.
            if (target != 0 && target == mouseDownTarget_) {
                auto click = std::make_unique<MouseEvent>(EventType::Click, target,
                                                            MouseButton::Left, mouseX_, mouseY_);
                Dispatch(click.get());
            }
            mouseDownTarget_ = 0;
        }
    }

    if (rightDown != prevMouseButtonState_[1]) {
        ComponentId target = HitTest(root, mouseX_, mouseY_);
        EventType type = rightDown ? EventType::MouseDown : EventType::MouseUp;
        auto event = std::make_unique<MouseEvent>(type, target, MouseButton::Right, mouseX_, mouseY_);
        Dispatch(event.get());
    }

    if (middleDown != prevMouseButtonState_[2]) {
        ComponentId target = HitTest(root, mouseX_, mouseY_);
        EventType type = middleDown ? EventType::MouseDown : EventType::MouseUp;
        auto event = std::make_unique<MouseEvent>(type, target, MouseButton::Middle, mouseX_, mouseY_);
        Dispatch(event.get());
    }

    keyboardState_.clear();
    for (int keyCode = 1; keyCode < 255; ++keyCode) {
        if (platformKeyboard_->IsKeyDown(keyCode)) {
            keyboardState_[keyCode] = true;
        }
    }

    if (focusedComponent_ != 0) {
        for (const auto& [keyCode, isDown] : keyboardState_) {
            if (prevKeyboardState_.find(keyCode) == prevKeyboardState_.end()) {
                auto event = std::make_unique<KeyboardEvent>(EventType::KeyDown, focusedComponent_, keyCode);
                Dispatch(event.get());
            }
        }
        for (const auto& [keyCode, isDown] : prevKeyboardState_) {
            if (keyboardState_.find(keyCode) == keyboardState_.end()) {
                auto event = std::make_unique<KeyboardEvent>(EventType::KeyUp, focusedComponent_, keyCode);
                Dispatch(event.get());
            }
        }
    }
}

bool EventDispatcher::IsMouseButtonDown(MouseButton btn) const {
    switch (btn) {
        case MouseButton::Left:   return mouseButtonState_[0];
        case MouseButton::Right:  return mouseButtonState_[1];
        case MouseButton::Middle: return mouseButtonState_[2];
        default: return false;
    }
}

ComponentId EventDispatcher::HitTest(IComponent* root, int x, int y) {
    if (!root || !layoutEngine_) return 0;
    
    auto layoutRoot = layoutEngine_->Root();
    if (!layoutRoot) return 0;

    return HitTestRecursive(root, layoutRoot, x, y);
}

ComponentId EventDispatcher::HitTestRecursive(IComponent* node, const ILayoutNode* layoutNode,
                                              int x, int y) {
    if (!node || !layoutNode) return 0;

    // Bounds check (simplified: use layout rect)
    const auto rect = layoutNode->Rect();
    if (x < rect.x || x >= rect.x + rect.width || y < rect.y || y >= rect.y + rect.height) {
        return 0;
    }

    // Test children first (painter's algorithm: top-most wins)
    for (const auto& child : node->Children()) {
        auto childLayout = layoutEngine_->FindNode(child->Id());
        if (auto hit = HitTestRecursive(child, childLayout, x, y)) {
            return hit;
        }
    }

    // If no child hit, return this node
    return node->Id();
}

} // namespace events
} // namespace ui
} // namespace avalang

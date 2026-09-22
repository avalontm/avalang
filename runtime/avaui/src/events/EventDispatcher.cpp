#include "events/EventDispatcher.h"
#include "events/Event.h"
#include "events/HitTest.h"
#include "components/IComponent.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "runtime/avalang/platform/interfaces/services/ui/IMouse.h"
#include "runtime/avalang/platform/interfaces/services/ui/IKeyboard.h"
#include "runtime/avalang/platform/interfaces/services/ui/IWheel.h"
#include "runtime/avalang/platform/interfaces/services/ui/ITextInput.h"
#include "runtime/avalang/platform/interfaces/services/ui/ITouch.h"
#include "runtime/avalang/platform/interfaces/services/ui/IIme.h"
#include "runtime/avalang/platform/interfaces/services/ui/IPlatformServices.h"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace avalang {
namespace ui {
namespace events {

using namespace ava::platform::ui;

namespace {
constexpr uint64_t kMousePointerTouchId = ~static_cast<uint64_t>(0);

uint64_t NowMs() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}
}

EventDispatcher::EventDispatcher() {
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
        auto blur = std::make_unique<FocusEvent>(EventType::Blur, previous, id);
        Dispatch(blur.get());
    }
    if (id != 0) {
        auto focus = std::make_unique<FocusEvent>(EventType::Focus, id, previous);
        Dispatch(focus.get());
    }
}

void EventDispatcher::Dispatch(IEvent* event) {
    if (!event) return;

    for (const auto& sub : subscriptions_) {
        if (sub.target == event->Target() && sub.type == event->Type()) {
            sub.handler->OnEvent(event);
            if (!event->IsPropagating()) return;
        }
    }
}

void EventDispatcher::DispatchChange(ComponentId target, PropertyValue value) {
    auto change = std::make_unique<ChangeEvent>(target, std::move(value));
    Dispatch(change.get());
}

void EventDispatcher::PollInput(IComponent* root) {
    if (!root || !platformMouse_ || !platformKeyboard_) return;

    EnforceFocusTrap(root);

    prevPointerX_ = pointerX_;
    prevPointerY_ = pointerY_;
    std::memcpy(prevPointerButtonState_, pointerButtonState_, sizeof(pointerButtonState_));
    prevKeyboardState_ = keyboardState_;

    int newX, newY;
    platformMouse_->Position(newX, newY);
    pointerX_ = newX;
    pointerY_ = newY;

    bool leftDown   = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Left);
    bool rightDown  = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Right);
    bool middleDown = platformMouse_->IsButtonDown(ava::platform::ui::MouseButton::Middle);

    pointerButtonState_[0] = leftDown;
    pointerButtonState_[1] = rightDown;
    pointerButtonState_[2] = middleDown;

    ComponentId hitTarget = HitTest(root, pointerX_, pointerY_);

    if (pointerX_ != prevPointerX_ || pointerY_ != prevPointerY_) {
        auto moveEvent = std::make_unique<PointerEvent>(EventType::PointerMove, hitTarget, PointerButton::None, pointerX_, pointerY_);
        Dispatch(moveEvent.get());

        if (activeTouches_.count(kMousePointerTouchId)) {
            UpdateGestureTracking(kMousePointerTouchId, pointerX_, pointerY_, NowMs(), root);
        }
    }

    if (hitTarget != hoveredTarget_) {
        if (hoveredTarget_ != 0) {
            auto leaveEvent = std::make_unique<PointerEvent>(EventType::PointerLeave, hoveredTarget_, PointerButton::None, pointerX_, pointerY_);
            Dispatch(leaveEvent.get());
        }
        if (hitTarget != 0) {
            auto enterEvent = std::make_unique<PointerEvent>(EventType::PointerEnter, hitTarget, PointerButton::None, pointerX_, pointerY_);
            Dispatch(enterEvent.get());
        }
        hoveredTarget_ = hitTarget;
    }

    if (leftDown != prevPointerButtonState_[0]) {
        ComponentId target = HitTest(root, pointerX_, pointerY_);
        EventType type = leftDown ? EventType::PointerDown : EventType::PointerUp;
        auto event = std::make_unique<PointerEvent>(type, target, PointerButton::Left, pointerX_, pointerY_);
        Dispatch(event.get());

        if (leftDown) {
            if (IsEnabledTarget(root, target)) {
                SetFocusedComponent(target);
            }
            pointerDownTarget_ = target;
            BeginGestureTracking(target, kMousePointerTouchId, pointerX_, pointerY_, NowMs());
        } else {
            if (target != 0 && target == pointerDownTarget_ && IsEnabledTarget(root, target)) {
                auto click = std::make_unique<PointerEvent>(EventType::Click, target,
                                                            PointerButton::Left, pointerX_, pointerY_);
                Dispatch(click.get());
                HandleOverlayDismiss(root, target);
            }
            pointerDownTarget_ = 0;
            EndGestureTracking(kMousePointerTouchId, pointerX_, pointerY_, NowMs(), false);
        }
    }

    if (rightDown != prevPointerButtonState_[1]) {
        ComponentId target = HitTest(root, pointerX_, pointerY_);
        EventType type = rightDown ? EventType::PointerDown : EventType::PointerUp;
        auto event = std::make_unique<PointerEvent>(type, target, PointerButton::Right, pointerX_, pointerY_);
        Dispatch(event.get());
    }

    if (middleDown != prevPointerButtonState_[2]) {
        ComponentId target = HitTest(root, pointerX_, pointerY_);
        EventType type = middleDown ? EventType::PointerDown : EventType::PointerUp;
        auto event = std::make_unique<PointerEvent>(type, target, PointerButton::Middle, pointerX_, pointerY_);
        Dispatch(event.get());
    }

    keyboardState_.clear();
    for (int keyCode = 1; keyCode < 255; ++keyCode) {
        if (platformKeyboard_->IsKeyDown(keyCode)) {
            keyboardState_[keyCode] = true;
        }
    }

    {
        bool shift = keyboardState_.count(16) > 0;
        bool ctrl = keyboardState_.count(17) > 0;
        bool alt = keyboardState_.count(18) > 0;

        for (const auto& [keyCode, isDown] : keyboardState_) {
            if (prevKeyboardState_.find(keyCode) == prevKeyboardState_.end()) {
                Key translated = keyTranslator_ ? keyTranslator_(keyCode) : Key::Unknown;

                if (focusedComponent_ != 0) {
                    auto event = std::make_unique<KeyboardEvent>(EventType::KeyDown, focusedComponent_, keyCode,
                                                                  translated, shift, ctrl, alt, false);
                    Dispatch(event.get());
                }

                if (translated == Key::Tab) {
                    // Tab must be able to move focus onto the first focusable
                    // component even when nothing is focused yet, so keyboard-only
                    // users are never stranded without a way to reach a control.
                    CycleFocus(root, shift);
                } else if (translated == Key::Escape && focusedComponent_ != 0) {
                    if (IComponent* overlay = events::FindTopmostBlockingOverlay(root)) {
                        if (events::IsDismissibleOverlay(overlay)) {
                            CloseOverlay(overlay);
                        }
                    }
                }
            }
        }
        if (focusedComponent_ != 0) {
            for (const auto& [keyCode, isDown] : prevKeyboardState_) {
                if (keyboardState_.find(keyCode) == keyboardState_.end()) {
                    Key translated = keyTranslator_ ? keyTranslator_(keyCode) : Key::Unknown;
                    auto event = std::make_unique<KeyboardEvent>(EventType::KeyUp, focusedComponent_, keyCode,
                                                                  translated, shift, ctrl, alt, false);
                    Dispatch(event.get());
                }
            }
        }
    }

    PollWheel(root);
    PollTextInput();
    PollIme();
    PollTouch(root);
}

void EventDispatcher::PollWheel(IComponent* root) {
    if (!platformWheel_) return;

    float dx = 0.0f, dy = 0.0f;
    platformWheel_->ConsumeDelta(dx, dy);
    if (dx == 0.0f && dy == 0.0f) return;

    ComponentId target = HitTest(root, pointerX_, pointerY_);
    if (target == 0) return;

    auto event = std::make_unique<WheelEvent>(target, pointerX_, pointerY_, dx, dy);
    Dispatch(event.get());
}

void EventDispatcher::PollTextInput() {
    if (!platformTextInput_ || focusedComponent_ == 0) return;

    std::string text = platformTextInput_->ConsumeCommittedText();
    if (text.empty()) return;

    auto event = std::make_unique<TextInputEvent>(focusedComponent_, std::move(text));
    Dispatch(event.get());
}

void EventDispatcher::PollIme() {
    if (!platformIme_ || focusedComponent_ == 0) return;

    ImeComposition composition = platformIme_->CurrentComposition();
    if (!composition.active && !imeWasComposing_) return;

    auto event = std::make_unique<ImeEvent>(focusedComponent_, composition.text,
                                            composition.cursor, composition.active);
    Dispatch(event.get());
    imeWasComposing_ = composition.active;
}

void EventDispatcher::PollTouch(IComponent* root) {
    if (!platformTouch_) return;

    auto nativePoints = platformTouch_->ActivePoints();
    uint64_t now = NowMs();

    std::unordered_map<ComponentId, std::vector<TouchPoint>> startedByTarget;
    std::unordered_map<ComponentId, std::vector<TouchPoint>> movedByTarget;
    std::unordered_map<uint64_t, bool> seen;

    for (const auto& np : nativePoints) {
        seen[np.id] = true;
        ComponentId target = HitTest(root, np.x, np.y);

        auto it = activeTouches_.find(np.id);
        if (it == activeTouches_.end()) {
            startedByTarget[target].push_back({np.id, np.x, np.y, TouchPhase::Started});
            BeginGestureTracking(target, np.id, np.x, np.y, now);
        } else if (it->second.x != np.x || it->second.y != np.y) {
            movedByTarget[it->second.target].push_back({np.id, np.x, np.y, TouchPhase::Moved});
            UpdateGestureTracking(np.id, np.x, np.y, now, root);
        }
    }

    std::vector<uint64_t> ended;
    for (const auto& [id, touch] : activeTouches_) {
        if (id == kMousePointerTouchId) continue;
        if (!seen.count(id)) ended.push_back(id);
    }

    for (const auto& [target, points] : startedByTarget) {
        auto event = std::make_unique<TouchEvent>(EventType::TouchStart, target, points);
        Dispatch(event.get());
    }

    for (const auto& [target, points] : movedByTarget) {
        auto event = std::make_unique<TouchEvent>(EventType::TouchMove, target, points);
        Dispatch(event.get());
    }

    std::unordered_map<ComponentId, std::vector<TouchPoint>> endedByTarget;
    for (uint64_t id : ended) {
        auto it = activeTouches_.find(id);
        int x = it != activeTouches_.end() ? it->second.x : 0;
        int y = it != activeTouches_.end() ? it->second.y : 0;
        ComponentId target = it != activeTouches_.end() ? it->second.target : HitTest(root, x, y);
        endedByTarget[target].push_back({id, x, y, TouchPhase::Ended});
    }

    for (const auto& [target, points] : endedByTarget) {
        auto event = std::make_unique<TouchEvent>(EventType::TouchEnd, target, points);
        Dispatch(event.get());
    }

    for (uint64_t id : ended) {
        auto it = activeTouches_.find(id);
        int x = it != activeTouches_.end() ? it->second.x : 0;
        int y = it != activeTouches_.end() ? it->second.y : 0;
        EndGestureTracking(id, x, y, now, false);
    }
}

void EventDispatcher::BeginGestureTracking(ComponentId target, uint64_t id, int x, int y, uint64_t timestamp) {
    TrackedTouch touch;
    touch.target = target;
    touch.startX = x;
    touch.startY = y;
    touch.x = x;
    touch.y = y;
    touch.startTime = timestamp;
    touch.longPressFired = false;
    activeTouches_[id] = touch;
}

void EventDispatcher::UpdateGestureTracking(uint64_t id, int x, int y, uint64_t timestamp, IComponent* root) {
    auto it = activeTouches_.find(id);
    if (it == activeTouches_.end()) return;

    TrackedTouch& touch = it->second;
    int dx = x - touch.startX;
    int dy = y - touch.startY;
    int distance = dx * dx + dy * dy;

    if (distance >= panMinMovement_ * panMinMovement_) {
        auto event = std::make_unique<GestureEvent>(touch.target, GestureType::Pan, x, y,
                                                     static_cast<float>(dx), static_cast<float>(dy));
        Dispatch(event.get());
    }

    if (!touch.longPressFired && distance < tapMaxMovement_ * tapMaxMovement_ &&
        (timestamp - touch.startTime) >= longPressMs_) {
        auto event = std::make_unique<GestureEvent>(touch.target, GestureType::LongPress, x, y);
        Dispatch(event.get());
        touch.longPressFired = true;
    }

    touch.x = x;
    touch.y = y;
}

void EventDispatcher::EndGestureTracking(uint64_t id, int x, int y, uint64_t timestamp, bool cancelled) {
    auto it = activeTouches_.find(id);
    if (it == activeTouches_.end()) return;

    TrackedTouch touch = it->second;
    activeTouches_.erase(it);

    if (cancelled) return;

    int dx = x - touch.startX;
    int dy = y - touch.startY;
    int distance = dx * dx + dy * dy;

    if (!touch.longPressFired && distance < tapMaxMovement_ * tapMaxMovement_) {
        auto event = std::make_unique<GestureEvent>(touch.target, GestureType::Tap, x, y);
        Dispatch(event.get());
    }
}

bool EventDispatcher::IsPointerButtonDown(PointerButton button) const {
    switch (button) {
        case PointerButton::Left:   return pointerButtonState_[0];
        case PointerButton::Right:  return pointerButtonState_[1];
        case PointerButton::Middle: return pointerButtonState_[2];
        default: return false;
    }
}

ComponentId EventDispatcher::HitTest(IComponent* root, int x, int y) {
    if (!root || !layoutEngine_) return 0;

    auto layoutRoot = layoutEngine_->Root();
    if (!layoutRoot) return 0;

    if (IComponent* overlay = events::FindTopmostBlockingOverlay(root)) {
        const ILayoutNode* overlayLayout = layoutEngine_->FindNode(overlay->Id());
        if (overlayLayout) {
            ComponentId hit = HitTestRecursive(overlay, overlayLayout, x, y);
            if (hit != 0) {
                return hit;
            }
        }
        return overlay->Id();
    }

    return HitTestRecursive(root, layoutRoot, x, y);
}

bool EventDispatcher::IsWithinComponentBounds(IComponent* component, int x, int y) const {
    if (!component || !layoutEngine_) return false;
    const ILayoutNode* layoutNode = layoutEngine_->FindNode(component->Id());
    if (!layoutNode) return false;
    const auto rect = layoutNode->Rect();
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

ComponentId EventDispatcher::HitTestRecursive(IComponent* node, const ILayoutNode* layoutNode,
                                              int x, int y) {
    if (!node || !layoutNode) return 0;

    const auto rect = layoutNode->Rect();
    if (x < rect.x || x >= rect.x + rect.width || y < rect.y || y >= rect.y + rect.height) {
        return 0;
    }

    if (events::OwnsInteractionSubtree(node)) {
        return node->Id();
    }

    for (const auto& child : node->Children()) {
        auto childLayout = layoutEngine_->FindNode(child->Id());
        if (auto hit = HitTestRecursive(child, childLayout, x, y)) {
            return hit;
        }
    }

    return node->Id();
}

void EventDispatcher::CloseOverlay(IComponent* overlay) {
    if (!overlay) return;
    if (overlay->HasProperty("isOpen")) {
        overlay->SetProperty("isOpen", PropertyValue(false));
    } else {
        overlay->SetProperty("overlay", PropertyValue(false));
    }
}

void EventDispatcher::HandleOverlayDismiss(IComponent* root, ComponentId clickedTarget) {
    IComponent* clicked = FindComponent(root, clickedTarget);
    if (!clicked || !events::IsBlockingOverlay(clicked) || !events::IsDismissibleOverlay(clicked)) {
        return;
    }
    if (IsWithinComponentBounds(clicked, pointerX_, pointerY_)) {
        return;
    }
    CloseOverlay(clicked);
}

void EventDispatcher::EnforceFocusTrap(IComponent* root) {
    IComponent* overlay = events::FindTopmostBlockingOverlay(root);
    if (!overlay) return;

    IComponent* current = FindComponent(root, focusedComponent_);
    if (current && events::IsDescendantOf(current, overlay)) {
        return;
    }

    std::vector<IComponent*> focusable;
    events::CollectFocusableDescendants(overlay, focusable);
    SetFocusedComponent(focusable.empty() ? overlay->Id() : focusable.front()->Id());
}

void EventDispatcher::CycleFocus(IComponent* root, bool backward) {
    IComponent* scopeRoot = root;
    if (IComponent* overlay = events::FindTopmostBlockingOverlay(root)) {
        scopeRoot = overlay;
    }

    std::vector<IComponent*> focusable;
    events::CollectFocusableDescendants(scopeRoot, focusable);
    if (focusable.empty()) return;

    int index = -1;
    for (size_t i = 0; i < focusable.size(); ++i) {
        if (focusable[i]->Id() == focusedComponent_) {
            index = static_cast<int>(i);
            break;
        }
    }

    int count = static_cast<int>(focusable.size());
    int next = backward ? index - 1 : index + 1;
    if (index < 0) {
        next = backward ? count - 1 : 0;
    } else if (next < 0) {
        next = count - 1;
    } else if (next >= count) {
        next = 0;
    }

    SetFocusedComponent(focusable[next]->Id());
}

IComponent* EventDispatcher::FindComponent(IComponent* node, ComponentId id) const {
    if (!node) return nullptr;
    if (node->Id() == id) return node;

    for (auto* child : node->Children()) {
        if (auto* found = FindComponent(child, id)) {
            return found;
        }
    }

    return nullptr;
}

bool EventDispatcher::IsEnabledTarget(IComponent* root, ComponentId id) const {
    if (id == 0) return true;

    IComponent* component = FindComponent(root, id);
    if (!component) return true;

    return events::ResolveInteractiveNode(component).enabled;
}

}
}
}

#include "events/Event.h"
#include <chrono>
#include <utility>

namespace avalang {
namespace ui {
namespace events {

static uint64_t GetTimestampMs() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
    return elapsed.count();
}

Event::Event(EventType type, ComponentId target)
    : type_(type), target_(target), timestamp_(GetTimestampMs()) {
}

PointerEvent::PointerEvent(EventType type, ComponentId target, PointerButton button, int x, int y)
    : Event(type, target), button_(button), x_(x), y_(y) {
}

PointerEvent::PointerEvent(EventType type, ComponentId target, int x, int y, int dx, int dy)
    : Event(type, target), button_(PointerButton::None), x_(x), y_(y), deltaX_(dx), deltaY_(dy) {
}

KeyboardEvent::KeyboardEvent(EventType type, ComponentId target, int keyCode, Key translatedKey,
                             bool shift, bool ctrl, bool alt, bool meta)
    : Event(type, target), keyCode_(keyCode), translatedKey_(translatedKey),
      shift_(shift), ctrl_(ctrl), alt_(alt), meta_(meta) {
}

WheelEvent::WheelEvent(ComponentId target, int x, int y, float deltaX, float deltaY)
    : Event(EventType::Wheel, target), x_(x), y_(y), deltaX_(deltaX), deltaY_(deltaY) {
}

TextInputEvent::TextInputEvent(ComponentId target, std::string text)
    : Event(EventType::TextInput, target), text_(std::move(text)) {
}

TouchEvent::TouchEvent(EventType type, ComponentId target, std::vector<TouchPoint> points)
    : Event(type, target), points_(std::move(points)) {
}

GestureEvent::GestureEvent(ComponentId target, GestureType gesture, int x, int y,
                           float translationX, float translationY, float scale)
    : Event(EventType::Gesture, target), gesture_(gesture), x_(x), y_(y),
      translationX_(translationX), translationY_(translationY), scale_(scale) {
}

ImeEvent::ImeEvent(ComponentId target, std::string compositionText, int cursor, bool composing)
    : Event(EventType::Ime, target), compositionText_(std::move(compositionText)),
      cursor_(cursor), composing_(composing) {
}

FocusEvent::FocusEvent(EventType type, ComponentId target, ComponentId relatedTarget)
    : Event(type, target), relatedTarget_(relatedTarget) {
}

ChangeEvent::ChangeEvent(ComponentId target, PropertyValue value)
    : Event(EventType::Change, target), value_(std::move(value)) {
}

}
}
}

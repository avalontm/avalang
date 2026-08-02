#include "events/Event.h"
#include <chrono>

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

MouseEvent::MouseEvent(EventType type, ComponentId target, MouseButton button, int x, int y)
    : Event(type, target), button_(button), x_(x), y_(y) {
}

MouseEvent::MouseEvent(EventType type, ComponentId target, int x, int y, int dx, int dy)
    : Event(type, target), button_(MouseButton::None), x_(x), y_(y), deltaX_(dx), deltaY_(dy) {
}

KeyboardEvent::KeyboardEvent(EventType type, ComponentId target, int keyCode,
                             bool shift, bool ctrl, bool alt, bool meta)
    : Event(type, target), keyCode_(keyCode), shift_(shift), ctrl_(ctrl), alt_(alt), meta_(meta) {
}

} // namespace events
} // namespace ui
} // namespace avalang

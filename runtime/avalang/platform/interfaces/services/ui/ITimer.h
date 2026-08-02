#ifndef AVA_PLATFORM_SERVICES_UI_ITIMER_H
#define AVA_PLATFORM_SERVICES_UI_ITIMER_H

#include <cstdint>
#include <functional>

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Required for async/await, animations, dispatcher and
// delayed/frame scheduling (see docs/Platform_Foundation.md).
class ITimer {
public:
    virtual ~ITimer() = default;

    // Schedules callback to run once after delayMs. Returns a handle that
    // can be passed to Cancel().
    virtual uint64_t ScheduleOnce(uint32_t delayMs, std::function<void()> callback) = 0;

    // Schedules callback to run repeatedly every intervalMs.
    virtual uint64_t ScheduleRepeating(uint32_t intervalMs, std::function<void()> callback) = 0;

    virtual void Cancel(uint64_t handle) = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_ITIMER_H

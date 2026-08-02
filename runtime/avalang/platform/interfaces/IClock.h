#ifndef AVA_PLATFORM_ICLOCK_H
#define AVA_PLATFORM_ICLOCK_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IClock.
#include "PAL_ABI.h"

#include <cstdint>

namespace ava {
namespace platform {

class IClock {
public:
    virtual ~IClock() = default;

    // Wall-clock time, milliseconds since Unix epoch.
    virtual int64_t NowMs() const = 0;

    // Monotonic high-resolution timer, nanoseconds. Not tied to wall time;
    // only valid for measuring elapsed durations.
    virtual int64_t HighResNowNs() const = 0;

    virtual void SleepMs(uint32_t milliseconds) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_ICLOCK_H

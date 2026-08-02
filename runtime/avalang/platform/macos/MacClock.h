#ifndef AVA_PLATFORM_MAC_CLOCK_H
#define AVA_PLATFORM_MAC_CLOCK_H

#include "../interfaces/IClock.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB. TODO: back with clock_gettime(CLOCK_REALTIME/CLOCK_MONOTONIC).
class MacClock : public IClock {
public:
    int64_t NowMs() const override;
    int64_t HighResNowNs() const override;
    void SleepMs(uint32_t milliseconds) override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_CLOCK_H

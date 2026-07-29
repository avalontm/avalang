#ifndef AVA_PLATFORM_WIN_CLOCK_H
#define AVA_PLATFORM_WIN_CLOCK_H

#include "../interfaces/IClock.h"

namespace ava {
namespace platform {
namespace windows {

class WinClock : public IClock {
public:
    int64_t NowMs() const override;
    int64_t HighResNowNs() const override;
    void SleepMs(uint32_t milliseconds) override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_CLOCK_H

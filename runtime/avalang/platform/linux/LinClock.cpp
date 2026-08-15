#include "LinClock.h"

#include <time.h>
#include <cstdint>

namespace ava {
namespace platform {
namespace linux_ {

int64_t LinClock::NowMs() const {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

int64_t LinClock::HighResNowNs() const {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

void LinClock::SleepMs(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, nullptr);
}

} // namespace linux_
} // namespace platform
} // namespace ava

#include "LinClock.h"

// STUB implementation -- returns zero/no-op instead of real OS time.
// TODO(Phase 5): clock_gettime + nanosleep,
// mirroring core/platform/windows/LinClock.cpp.

namespace ava {
namespace platform {
namespace linux_ {

int64_t LinClock::NowMs() const {
    return 0;
}

int64_t LinClock::HighResNowNs() const {
    return 0;
}

void LinClock::SleepMs(uint32_t /*milliseconds*/) {
    // Not implemented.
}

} // namespace linux_
} // namespace platform
} // namespace ava

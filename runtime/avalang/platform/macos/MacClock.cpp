#include "MacClock.h"

// STUB implementation -- returns zero/no-op instead of real OS time.
// TODO(Phase 6): clock_gettime + nanosleep,
// mirroring core/platform/windows/MacClock.cpp.

namespace ava {
namespace platform {
namespace macos_ {

int64_t MacClock::NowMs() const {
    return 0;
}

int64_t MacClock::HighResNowNs() const {
    return 0;
}

void MacClock::SleepMs(uint32_t /*milliseconds*/) {
    // Not implemented.
}

} // namespace macos_
} // namespace platform
} // namespace ava

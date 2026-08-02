#include "LinTimer.h"

namespace ava {
namespace platform {
namespace linux_ {

uint64_t LinTimer::ScheduleOnce(uint32_t /*delayMs*/, std::function<void()> /*callback*/) {
    // TODO(linux, en estudio): implementar con timerfd o std::thread+CV,
    // igual que WinTimer, cuando se retome el backend Linux.
    return 0;
}

void LinTimer::Cancel(uint64_t /*handle*/) {}

} // namespace linux_
} // namespace platform
} // namespace ava

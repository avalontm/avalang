#include "MacTimer.h"

namespace ava {
namespace platform {
namespace macos_ {

uint64_t MacTimer::ScheduleOnce(uint32_t /*delayMs*/, std::function<void()> /*callback*/) {
    // TODO(macos, en estudio): implementar con dispatch_after o
    // std::thread+CV, igual que WinTimer, cuando se retome el backend.
    return 0;
}

void MacTimer::Cancel(uint64_t /*handle*/) {}

} // namespace macos_
} // namespace platform
} // namespace ava

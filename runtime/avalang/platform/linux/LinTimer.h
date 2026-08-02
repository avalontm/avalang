#ifndef AVA_PLATFORM_LIN_TIMER_H
#define AVA_PLATFORM_LIN_TIMER_H

#include "../interfaces/ITimer.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB (ver PAL_PROGRESS.md "Alcance actual" -- Linux en estudio).
// Existe solo para que LinPlatform siga compilando tras el bump ABI v2
// (Fase 5). No implementa timers reales.
class LinTimer : public ITimer {
public:
    uint64_t ScheduleOnce(uint32_t delayMs, std::function<void()> callback) override;
    void Cancel(uint64_t handle) override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_TIMER_H

#ifndef AVA_PLATFORM_MAC_TIMER_H
#define AVA_PLATFORM_MAC_TIMER_H

#include "../interfaces/ITimer.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB (ver PAL_PROGRESS.md "Alcance actual" -- macOS en estudio).
// Existe solo para que MacPlatform siga compilando tras el bump ABI v2
// (Fase 5). No implementa timers reales.
class MacTimer : public ITimer {
public:
    uint64_t ScheduleOnce(uint32_t delayMs, std::function<void()> callback) override;
    void Cancel(uint64_t handle) override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_TIMER_H

#ifndef AVA_PLATFORM_BAREKERNEL_TIMER_H
#define AVA_PLATFORM_BAREKERNEL_TIMER_H

#include "../interfaces/ITimer.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

// Minimal timer. When CKM_CAP_THREADS is enabled, schedules callbacks on
// the barekernel thread pool (created lazily); otherwise fires inline.
class BareKernelTimer : public ITimer {
public:
    uint64_t ScheduleOnce(uint32_t delayMs, avastd::function<void()> callback) override;
    void Cancel(uint64_t handle) override;
};

}
}
}

#endif

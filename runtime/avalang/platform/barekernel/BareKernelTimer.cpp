#include "BareKernelTimer.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

#if CKM_CAP_THREADS && CKM_CAP_TIMERS
namespace {
struct TimerCb { avastd::function<void()> f; uint32_t delayMs; };
extern "C" void timer_trampoline(void* raw) {
    TimerCb* cb = static_cast<TimerCb*>(raw);
    avastd::function<void()> f = avastd::move(cb->f);
    uint32_t delayMs = cb->delayMs;
    delete cb;
    if (delayMs) ckm_sleep_ms(delayMs);
    if (f) f();
}
}
#endif

uint64_t BareKernelTimer::ScheduleOnce(uint32_t delayMs, avastd::function<void()> callback) {
#if CKM_CAP_THREADS && CKM_CAP_TIMERS
    if (!callback) return 0;
    // Spin off a fire-and-forget thread that invokes the callback. The CKM
    // does not yet expose a kernel-side timer queue, so we approximate it
    // with a one-shot thread. This is correct but heavyweight; when the
    // kernel grows a timer-wheel syscall, swap this for ckm_timer.
    auto* cb = new TimerCb{avastd::move(callback), delayMs};
    int h = ckm_thread_create(&timer_trampoline, cb);
    if (h < 0) { delete cb; return 0; }
    return 1;
#elif CKM_CAP_TIMERS
    if (!callback) return 0;
    ckm_sleep_ms(delayMs);
    callback();
    return 1;
#else
    (void)delayMs; (void)callback;
    return 0;
#endif
}

void BareKernelTimer::Cancel(uint64_t handle) {
    // No timer-queue primitive in CKM yet -- cancellation is a no-op. Callers
    // that need real cancellation should guard their callbacks with a flag.
    (void)handle;
}

}
}
}

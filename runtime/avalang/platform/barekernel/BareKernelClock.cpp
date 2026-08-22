#include "BareKernelClock.h"
#include "ckm_contract.h"

namespace ava {
namespace platform {
namespace barekernel {

int64_t BareKernelClock::NowMs() const {
    return ckm_now_ms();
}

int64_t BareKernelClock::HighResNowNs() const {
    return ckm_highres_now_ns();
}

void BareKernelClock::SleepMs(uint32_t milliseconds) {
    ckm_sleep_ms(milliseconds);
}

}
}
}

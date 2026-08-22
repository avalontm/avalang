#ifndef AVA_PLATFORM_BAREKERNEL_CLOCK_H
#define AVA_PLATFORM_BAREKERNEL_CLOCK_H

#include "../interfaces/IClock.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelClock : public IClock {
public:
    int64_t NowMs() const override;
    int64_t HighResNowNs() const override;
    void SleepMs(uint32_t milliseconds) override;
};

}
}
}

#endif

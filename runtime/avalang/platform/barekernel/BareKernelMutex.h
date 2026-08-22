#ifndef AVA_PLATFORM_BAREKERNEL_MUTEX_H
#define AVA_PLATFORM_BAREKERNEL_MUTEX_H

#include "../interfaces/IMutex.h"
#include "../interfaces/PAL_ABI.h"
#include "ckm_contract.h"

#if CKM_CAP_LIBSTDCPP
#  include <atomic>
#  include <cstdint>
#endif

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelMutex : public IMutex {
public:
    BareKernelMutex();
    ~BareKernelMutex() override;
    void Lock() override;
    void Unlock() override;
    bool TryLock() override;
private:
#if CKM_CAP_MUTEX
    CkmMutex raw_;
#elif CKM_CAP_LIBSTDCPP
    avastd::atomic<uint32_t> spin_;
#else
    volatile uint32_t spin_;
#endif
};

}
}
}

#endif

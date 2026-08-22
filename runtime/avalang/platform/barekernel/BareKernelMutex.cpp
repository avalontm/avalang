#include "BareKernelMutex.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"

namespace ava {
namespace platform {
namespace barekernel {

BareKernelMutex::BareKernelMutex() {
#if CKM_CAP_MUTEX
    ckm_mutex_init(&raw_);
#elif CKM_CAP_LIBSTDCPP
    spin_.store(0, avastd::memory_order_relaxed);
#else
    spin_ = 0;
#endif
}

BareKernelMutex::~BareKernelMutex() {
#if CKM_CAP_MUTEX
    // ckm has no destroy primitive; nothing to do.
#endif
}

void BareKernelMutex::Lock() {
#if CKM_CAP_MUTEX
    ckm_mutex_lock(&raw_);
#elif CKM_CAP_LIBSTDCPP
    uint32_t expected = 0;
    while (!spin_.compare_exchange_weak(expected, 1,
                                        avastd::memory_order_acquire,
                                        avastd::memory_order_relaxed)) {
        expected = 0;
        ckm_sleep_ms(1);
    }
#else
    uint32_t expected = 0;
    while (!__atomic_compare_exchange_n(&spin_, &expected, 1, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        expected = 0;
        ckm_sleep_ms(1);
    }
#endif
}

void BareKernelMutex::Unlock() {
#if CKM_CAP_MUTEX
    ckm_mutex_unlock(&raw_);
#elif CKM_CAP_LIBSTDCPP
    spin_.store(0, avastd::memory_order_release);
#else
    __atomic_store_n(&spin_, 0, __ATOMIC_RELEASE);
#endif
}

bool BareKernelMutex::TryLock() {
#if CKM_CAP_MUTEX
    return ckm_mutex_trylock(&raw_) == 0;
#elif CKM_CAP_LIBSTDCPP
    uint32_t expected = 0;
    return spin_.compare_exchange_strong(expected, 1,
                                         avastd::memory_order_acquire,
                                         avastd::memory_order_relaxed);
#else
    uint32_t expected = 0;
    return __atomic_compare_exchange_n(&spin_, &expected, 1, 0,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
#endif
}

}
}
}

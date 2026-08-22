#ifndef AVA_PLATFORM_BAREKERNEL_THREAD_H
#define AVA_PLATFORM_BAREKERNEL_THREAD_H

#include "../interfaces/IThread.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelInlineThread : public IThread {
public:
    void Join() override {}
    bool Joinable() const override { return false; }
    uint64_t Id() const override { return 0; }
};

class BareKernelRealThread : public IThread {
public:
    explicit BareKernelRealThread(int handle) : handle_(handle) {}
    ~BareKernelRealThread() override;
    void Join() override;
    bool Joinable() const override;
    uint64_t Id() const override { return static_cast<uint64_t>(handle_); }
private:
    int handle_;
};

class BareKernelThreadFactory : public IThreadFactory {
public:
    IThread* CreateThread(ThreadFunc func) override;
    void SleepMs(uint32_t milliseconds) override;
    uint64_t CurrentThreadId() const override;
};

}
}
}

#endif

#include "BareKernelThread.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

namespace {
struct ThreadCtx { ThreadFunc func; };
extern "C" void trampoline(void* raw) {
    auto* ctx = static_cast<ThreadCtx*>(raw);
    ThreadFunc f = avastd::move(ctx->func);
    ctx->~ThreadCtx();
    if (f) f();
}
}

#if CKM_CAP_THREADS
BareKernelRealThread::~BareKernelRealThread() {
    if (handle_ > 0) ckm_thread_join(handle_);
}

void BareKernelRealThread::Join() {
    if (handle_ > 0) { ckm_thread_join(handle_); handle_ = 0; }
}

bool BareKernelRealThread::Joinable() const {
    return handle_ > 0;
}
#else
BareKernelRealThread::~BareKernelRealThread() = default;
void BareKernelRealThread::Join() {}
bool BareKernelRealThread::Joinable() const { return false; }
#endif

IThread* BareKernelThreadFactory::CreateThread(ThreadFunc func) {
#if CKM_CAP_THREADS
    auto* ctx = new ThreadCtx{avastd::move(func)};
    int h = ckm_thread_create(&trampoline, ctx);
    if (h < 0) { delete ctx; return new BareKernelInlineThread(); }
    return new BareKernelRealThread(h);
#else
    if (func) func();
    return new BareKernelInlineThread();
#endif
}

void BareKernelThreadFactory::SleepMs(uint32_t milliseconds) {
    ckm_sleep_ms(milliseconds);
}

uint64_t BareKernelThreadFactory::CurrentThreadId() const {
#if CKM_CAP_THREADS
    return static_cast<uint64_t>(ckm_getpid());
#else
    return static_cast<uint64_t>(ckm_getpid());
#endif
}

}
}
}

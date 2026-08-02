#include "LinThread.h"

// STUB implementation -- does not spawn a real OS thread yet.
// TODO(Phase 5): pthread_create/pthread_join/nanosleep,
// mirroring core/platform/windows/LinThread.cpp.

namespace ava {
namespace platform {
namespace linux_ {

LinThread::LinThread(ThreadFunc func) : func_(std::move(func)) {
}

LinThread::~LinThread() = default;

void LinThread::Join() {
    // Not implemented: no real thread was started.
}

bool LinThread::Joinable() const {
    return false;
}

uint64_t LinThread::Id() const {
    return 0;
}

IThread* LinThreadFactory::CreateThread(ThreadFunc func) {
    return new LinThread(std::move(func));
}

void LinThreadFactory::SleepMs(uint32_t /*milliseconds*/) {
    // Not implemented.
}

uint64_t LinThreadFactory::CurrentThreadId() const {
    return 0;
}

} // namespace linux_
} // namespace platform
} // namespace ava

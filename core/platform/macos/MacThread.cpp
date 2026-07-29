#include "MacThread.h"

// STUB implementation -- does not spawn a real OS thread yet.
// TODO(Phase 6): pthread_create/pthread_join/nanosleep,
// mirroring core/platform/windows/MacThread.cpp.

namespace ava {
namespace platform {
namespace macos_ {

MacThread::MacThread(ThreadFunc func) : func_(std::move(func)) {
}

MacThread::~MacThread() = default;

void MacThread::Join() {
    // Not implemented: no real thread was started.
}

bool MacThread::Joinable() const {
    return false;
}

uint64_t MacThread::Id() const {
    return 0;
}

IThread* MacThreadFactory::CreateThread(ThreadFunc func) {
    return new MacThread(std::move(func));
}

void MacThreadFactory::SleepMs(uint32_t /*milliseconds*/) {
    // Not implemented.
}

uint64_t MacThreadFactory::CurrentThreadId() const {
    return 0;
}

} // namespace macos_
} // namespace platform
} // namespace ava

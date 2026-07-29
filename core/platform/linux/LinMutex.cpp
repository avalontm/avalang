#include "LinMutex.h"

// STUB implementation -- not thread-safe yet.
// TODO(Phase 5): pthread_mutex_init/lock/unlock/trylock,
// mirroring core/platform/windows/LinMutex.cpp.

namespace ava {
namespace platform {
namespace linux_ {

LinMutex::LinMutex() = default;
LinMutex::~LinMutex() = default;

void LinMutex::Lock() {
    // Not implemented.
}

void LinMutex::Unlock() {
    // Not implemented.
}

bool LinMutex::TryLock() {
    return false;
}

} // namespace linux_
} // namespace platform
} // namespace ava

#include "MacMutex.h"

// STUB implementation -- not thread-safe yet.
// TODO(Phase 6): pthread_mutex_init/lock/unlock/trylock,
// mirroring core/platform/windows/MacMutex.cpp.

namespace ava {
namespace platform {
namespace macos_ {

MacMutex::MacMutex() = default;
MacMutex::~MacMutex() = default;

void MacMutex::Lock() {
    // Not implemented.
}

void MacMutex::Unlock() {
    // Not implemented.
}

bool MacMutex::TryLock() {
    return false;
}

} // namespace macos_
} // namespace platform
} // namespace ava

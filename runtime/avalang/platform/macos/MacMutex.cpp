#include "MacMutex.h"

#include <mutex>

namespace ava {
namespace platform {
namespace macos_ {

MacMutex::MacMutex() : mutex_(new std::mutex()) {
}

MacMutex::~MacMutex() {
    delete mutex_;
    mutex_ = nullptr;
}

void MacMutex::Lock() {
    if (mutex_) mutex_->lock();
}

void MacMutex::Unlock() {
    if (mutex_) mutex_->unlock();
}

bool MacMutex::TryLock() {
    if (!mutex_) return false;
    return mutex_->try_lock();
}

} // namespace macos_
} // namespace platform
} // namespace ava

#include "LinMutex.h"

#include <mutex>

namespace ava {
namespace platform {
namespace linux_ {

LinMutex::LinMutex() : mutex_(new std::mutex()) {
}

LinMutex::~LinMutex() {
    delete mutex_;
    mutex_ = nullptr;
}

void LinMutex::Lock() {
    if (mutex_) mutex_->lock();
}

void LinMutex::Unlock() {
    if (mutex_) mutex_->unlock();
}

bool LinMutex::TryLock() {
    if (!mutex_) return false;
    return mutex_->try_lock();
}

} // namespace linux_
} // namespace platform
} // namespace ava

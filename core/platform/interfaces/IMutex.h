#ifndef AVA_PLATFORM_IMUTEX_H
#define AVA_PLATFORM_IMUTEX_H

namespace ava {
namespace platform {

class IMutex {
public:
    virtual ~IMutex() = default;

    virtual void Lock() = 0;
    virtual void Unlock() = 0;
    virtual bool TryLock() = 0;
};

// RAII helper, platform-agnostic (implemented once against IMutex, not
// per-backend).
class MutexLockGuard {
public:
    explicit MutexLockGuard(IMutex& mutex) : mutex_(mutex) { mutex_.Lock(); }
    ~MutexLockGuard() { mutex_.Unlock(); }

    MutexLockGuard(const MutexLockGuard&) = delete;
    MutexLockGuard& operator=(const MutexLockGuard&) = delete;

private:
    IMutex& mutex_;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IMUTEX_H

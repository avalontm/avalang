#ifndef AVA_PLATFORM_LIN_MUTEX_H
#define AVA_PLATFORM_LIN_MUTEX_H

#include "../interfaces/IMutex.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB. TODO: back with pthread_mutex_t (Linux) / Darwin pthread mutex (macOS).
class LinMutex : public IMutex {
public:
    LinMutex();
    ~LinMutex() override;

    void Lock() override;
    void Unlock() override;
    bool TryLock() override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_MUTEX_H

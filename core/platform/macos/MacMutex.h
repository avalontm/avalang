#ifndef AVA_PLATFORM_MAC_MUTEX_H
#define AVA_PLATFORM_MAC_MUTEX_H

#include "../interfaces/IMutex.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB. TODO: back with pthread_mutex_t (Linux) / Darwin pthread mutex (macOS).
class MacMutex : public IMutex {
public:
    MacMutex();
    ~MacMutex() override;

    void Lock() override;
    void Unlock() override;
    bool TryLock() override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_MUTEX_H

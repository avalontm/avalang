#ifndef AVA_PLATFORM_MAC_MUTEX_H
#define AVA_PLATFORM_MAC_MUTEX_H

#include "../interfaces/IMutex.h"
#include <mutex>

namespace ava {
namespace platform {
namespace macos_ {

class MacMutex : public IMutex {
public:
    MacMutex();
    ~MacMutex() override;

    void Lock() override;
    void Unlock() override;
    bool TryLock() override;

private:
    std::mutex* mutex_;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_MUTEX_H

#ifndef AVA_PLATFORM_LIN_MUTEX_H
#define AVA_PLATFORM_LIN_MUTEX_H

#include "../interfaces/IMutex.h"
#include <mutex>

namespace ava {
namespace platform {
namespace linux_ {

class LinMutex : public IMutex {
public:
    LinMutex();
    ~LinMutex() override;

    void Lock() override;
    void Unlock() override;
    bool TryLock() override;

private:
    std::mutex* mutex_;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_MUTEX_H

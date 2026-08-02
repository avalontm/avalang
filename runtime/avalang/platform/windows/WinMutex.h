#ifndef AVA_PLATFORM_WIN_MUTEX_H
#define AVA_PLATFORM_WIN_MUTEX_H

#include "../interfaces/IMutex.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// See WinThread.h for why these are undefined: Windows.h's ANSI/Unicode
// dispatch macros collide with virtual method names in this platform layer.
#undef DeleteFile
#undef CreateDirectory
#undef GetCurrentDirectory
#undef SetCurrentDirectory
#undef CreateMutex

namespace ava {
namespace platform {
namespace windows {

class WinMutex : public IMutex {
public:
    WinMutex();
    ~WinMutex() override;

    void Lock() override;
    void Unlock() override;
    bool TryLock() override;

private:
    CRITICAL_SECTION cs_;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_MUTEX_H

#include "WinMutex.h"

namespace ava {
namespace platform {
namespace windows {

WinMutex::WinMutex() {
    InitializeCriticalSection(&cs_);
}

WinMutex::~WinMutex() {
    DeleteCriticalSection(&cs_);
}

void WinMutex::Lock() {
    EnterCriticalSection(&cs_);
}

void WinMutex::Unlock() {
    LeaveCriticalSection(&cs_);
}

bool WinMutex::TryLock() {
    return TryEnterCriticalSection(&cs_) != 0;
}

} // namespace windows
} // namespace platform
} // namespace ava

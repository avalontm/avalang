#ifndef AVA_PLATFORM_WIN_THREAD_H
#define AVA_PLATFORM_WIN_THREAD_H

#include "../interfaces/IThread.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows.h #defines several ANSI/Unicode dispatch macros (DeleteFile,
// CreateDirectory, GetCurrentDirectory, SetCurrentDirectory, CreateMutex,
// ...) that collide with identically-named virtual methods declared across
// this platform layer. WinThread.h is the first header in the include chain
// (via WinPlatform.h) to pull in Windows.h, so undefine them here to keep
// downstream headers (WinFileSystem.h, WinEnvironment.h, WinPlatform.h)
// seeing the literal, un-suffixed names they were written against.
#undef DeleteFile
#undef CreateDirectory
#undef GetCurrentDirectory
#undef SetCurrentDirectory
#undef CreateMutex

namespace ava {
namespace platform {
namespace windows {

class WinThread : public IThread {
public:
    explicit WinThread(ThreadFunc func);
    ~WinThread() override;

    void Join() override;
    bool Joinable() const override;
    uint64_t Id() const override;

private:
    static DWORD WINAPI ThreadTrampoline(LPVOID param);

    HANDLE handle_ = nullptr;
    DWORD thread_id_ = 0;
    ThreadFunc func_;
    bool joined_ = false;
};

class WinThreadFactory : public IThreadFactory {
public:
    IThread* CreateThread(ThreadFunc func) override;
    void SleepMs(uint32_t milliseconds) override;
    uint64_t CurrentThreadId() const override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_THREAD_H

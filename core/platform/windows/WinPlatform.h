#ifndef AVA_PLATFORM_WIN_PLATFORM_H
#define AVA_PLATFORM_WIN_PLATFORM_H

#include "../interfaces/IPlatform.h"
#include "WinFileSystem.h"
#include "WinThread.h"
#include "WinClock.h"
#include "WinLibrary.h"
#include "WinConsole.h"
#include "WinEnvironment.h"
#include "WinProcess.h"

namespace ava {
namespace platform {
namespace windows {

class WinPlatform : public IPlatform {
public:
    IFileSystem& FileSystem() override { return file_system_; }
    IThreadFactory& Threads() override { return thread_factory_; }
    IClock& Clock() override { return clock_; }
    ILibraryLoader& Libraries() override { return library_loader_; }
    IConsole& Console() override { return console_; }
    IEnvironment& Environment() override { return environment_; }
    IProcess& Process() override { return process_; }

    IMutex* CreateMutex() override;

private:
    WinFileSystem file_system_;
    WinThreadFactory thread_factory_;
    WinClock clock_;
    WinLibraryLoader library_loader_;
    WinConsole console_;
    WinEnvironment environment_;
    WinProcess process_;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_PLATFORM_H

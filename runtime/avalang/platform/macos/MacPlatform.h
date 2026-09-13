#ifndef AVA_PLATFORM_MAC_PLATFORM_H
#define AVA_PLATFORM_MAC_PLATFORM_H

#include "../interfaces/IPlatform.h"
#include "MacFileSystem.h"
#include "MacThread.h"
#include "MacClock.h"
#include "MacLibrary.h"
#include "MacConsole.h"
#include "MacEnvironment.h"
#include "MacProcess.h"
#include "MacTimer.h"

namespace ava {
namespace platform {
namespace macos_ {

// Backend de macOS. FileSystem/Threads/Clock/Libraries/Console/Environment/
// Process/CreateMutex ya estan implementados contra POSIX/Darwin real (ver
// PLAN_LIBRERIAS_NATIVAS_MULTIPLATAFORMA.md, Fases 1-2). Timer() sigue
// siendo un stub (ver MacTimer.h), no forma parte de ese plan.
class MacPlatform : public IPlatform {
public:
    IFileSystem& FileSystem() override { return file_system_; }
    IThreadFactory& Threads() override { return thread_factory_; }
    IClock& Clock() override { return clock_; }
    ILibraryLoader& Libraries() override { return library_loader_; }
    IConsole& Console() override { return console_; }
    IEnvironment& Environment() override { return environment_; }
    IProcess& Process() override { return process_; }
    ITimer& Timer() override { return timer_; }

    IMutex* CreateMutex() override;

private:
    MacFileSystem file_system_;
    MacThreadFactory thread_factory_;
    MacClock clock_;
    MacLibraryLoader library_loader_;
    MacConsole console_;
    MacEnvironment environment_;
    MacProcess process_;
    MacTimer timer_;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_PLATFORM_H

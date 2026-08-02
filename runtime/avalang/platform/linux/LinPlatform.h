#ifndef AVA_PLATFORM_LIN_PLATFORM_H
#define AVA_PLATFORM_LIN_PLATFORM_H

#include "../interfaces/IPlatform.h"
#include "LinFileSystem.h"
#include "LinThread.h"
#include "LinClock.h"
#include "LinLibrary.h"
#include "LinConsole.h"
#include "LinEnvironment.h"
#include "LinProcess.h"
#include "LinTimer.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB backend. Compiles and wires into IPlatform so the rest of the
// codebase (Runtime/VM/Compiler) can target this OS today; every
// individual method still needs its real implementation (see the
// per-file TODOs in this directory) before this is usable at runtime.
class LinPlatform : public IPlatform {
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
    LinFileSystem file_system_;
    LinThreadFactory thread_factory_;
    LinClock clock_;
    LinLibraryLoader library_loader_;
    LinConsole console_;
    LinEnvironment environment_;
    LinProcess process_;
    LinTimer timer_;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_PLATFORM_H

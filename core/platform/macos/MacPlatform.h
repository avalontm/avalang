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

namespace ava {
namespace platform {
namespace macos_ {

// STUB backend. Compiles and wires into IPlatform so the rest of the
// codebase (Runtime/VM/Compiler) can target this OS today; every
// individual method still needs its real implementation (see the
// per-file TODOs in this directory) before this is usable at runtime.
class MacPlatform : public IPlatform {
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
    MacFileSystem file_system_;
    MacThreadFactory thread_factory_;
    MacClock clock_;
    MacLibraryLoader library_loader_;
    MacConsole console_;
    MacEnvironment environment_;
    MacProcess process_;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_PLATFORM_H

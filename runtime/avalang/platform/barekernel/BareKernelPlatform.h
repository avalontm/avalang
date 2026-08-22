#ifndef AVA_PLATFORM_BAREKERNEL_PLATFORM_H
#define AVA_PLATFORM_BAREKERNEL_PLATFORM_H

#include "../interfaces/IPlatform.h"
#include "BareKernelFileSystem.h"
#include "BareKernelThread.h"
#include "BareKernelClock.h"
#include "BareKernelLibrary.h"
#include "BareKernelConsole.h"
#include "BareKernelEnvironment.h"
#include "BareKernelProcess.h"
#include "BareKernelTimer.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelPlatform : public IPlatform {
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
    BareKernelFileSystem   file_system_;
    BareKernelThreadFactory thread_factory_;
    BareKernelClock        clock_;
    BareKernelLibraryLoader library_loader_;
    BareKernelConsole      console_;
    BareKernelEnvironment  environment_;
    BareKernelProcess      process_;
    BareKernelTimer        timer_;
};

} // namespace barekernel
} // namespace platform
} // namespace ava

#endif

#ifndef AVA_PLATFORM_IPLATFORM_H
#define AVA_PLATFORM_IPLATFORM_H

#include "IFileSystem.h"
#include "IThread.h"
#include "IMutex.h"
#include "IClock.h"
#include "ILibrary.h"
#include "IConsole.h"
#include "IEnvironment.h"
#include "IProcess.h"

namespace ava {
namespace platform {

// Single entry point handed to Runtime/VM/Compiler. Concrete instance is
// created by Platform::Create() (implemented per-OS in Phase 3/5/6),
// selected at compile time -- no runtime OS detection.
class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual IFileSystem& FileSystem() = 0;
    virtual IThreadFactory& Threads() = 0;
    virtual IClock& Clock() = 0;
    virtual ILibraryLoader& Libraries() = 0;
    virtual IConsole& Console() = 0;
    virtual IEnvironment& Environment() = 0;
    virtual IProcess& Process() = 0;

    // Creates a new mutex instance. Owned by the caller.
    virtual IMutex* CreateMutex() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPLATFORM_H

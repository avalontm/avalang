#ifndef AVA_PLATFORM_IPLATFORM_H
#define AVA_PLATFORM_IPLATFORM_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy. Changing the accessor set below is an ABI bump.
#include "PAL_ABI.h"

#include "IFileSystem.h"
#include "IThread.h"
#include "IMutex.h"
#include "IClock.h"
#include "ILibrary.h"
#include "IConsole.h"
#include "IEnvironment.h"
#include "IProcess.h"
#include "ITimer.h"

namespace ava {
namespace platform {

// Single entry point handed to Runtime/VM/Compiler. Concrete instance is
// created by Platform::Create() (implemented per-OS backend -- Windows is
// the only one in active development, see "Alcance actual" in
// PAL_PROGRESS.md), selected at compile time -- no runtime OS detection.
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
    // Fase 5 (Async Runtime). ABI v2, ver PAL_ABI.h.
    virtual ITimer& Timer() = 0;

    // Creates a new mutex instance. Owned by the caller.
    virtual IMutex* CreateMutex() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPLATFORM_H

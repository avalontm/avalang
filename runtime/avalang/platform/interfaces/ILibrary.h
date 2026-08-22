#ifndef AVA_PLATFORM_ILIBRARY_H
#define AVA_PLATFORM_ILIBRARY_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in ILibraryHandle / ILibraryLoader.
#include "PAL_ABI.h"

#include "../barekernel/stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {

// A loaded shared library handle (.dll / .so / .dylib).
class ILibraryHandle {
public:
    virtual ~ILibraryHandle() = default;

    // Returns nullptr if the symbol does not exist.
    virtual void* ResolveSymbol(const avastd::string& symbol_name) = 0;
};

// Used by the Extern/FFI system (see core/src/vm/vm_extern.h) to load
// native libraries without touching LoadLibrary/dlopen directly.
class ILibraryLoader {
public:
    virtual ~ILibraryLoader() = default;

    // Returns nullptr on failure. Caller owns the returned handle.
    virtual ILibraryHandle* Load(const avastd::string& library_name) = 0;
    virtual void Unload(ILibraryHandle* handle) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_ILIBRARY_H

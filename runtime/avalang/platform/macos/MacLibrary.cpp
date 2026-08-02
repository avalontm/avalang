#include "MacLibrary.h"

// STUB implementation -- never actually loads a shared library.
// TODO(Phase 6): dlopen/dlsym/dlclose,
// mirroring core/platform/windows/MacLibrary.cpp. Needed by the
// Extern/FFI system (core/src/vm/vm_extern.cpp).

namespace ava {
namespace platform {
namespace macos_ {

MacLibraryHandle::MacLibraryHandle(void* handle) : handle_(handle) {
}

void* MacLibraryHandle::ResolveSymbol(const std::string& /*symbol_name*/) {
    return nullptr;
}

ILibraryHandle* MacLibraryLoader::Load(const std::string& /*library_name*/) {
    return nullptr;
}

void MacLibraryLoader::Unload(ILibraryHandle* handle) {
    delete handle;
}

} // namespace macos_
} // namespace platform
} // namespace ava

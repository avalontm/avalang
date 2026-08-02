#include "LinLibrary.h"

// STUB implementation -- never actually loads a shared library.
// TODO(Phase 5): dlopen/dlsym/dlclose,
// mirroring core/platform/windows/LinLibrary.cpp. Needed by the
// Extern/FFI system (core/src/vm/vm_extern.cpp).

namespace ava {
namespace platform {
namespace linux_ {

LinLibraryHandle::LinLibraryHandle(void* handle) : handle_(handle) {
}

void* LinLibraryHandle::ResolveSymbol(const std::string& /*symbol_name*/) {
    return nullptr;
}

ILibraryHandle* LinLibraryLoader::Load(const std::string& /*library_name*/) {
    return nullptr;
}

void LinLibraryLoader::Unload(ILibraryHandle* handle) {
    delete handle;
}

} // namespace linux_
} // namespace platform
} // namespace ava

#include "MacLibrary.h"
#include <dlfcn.h>

namespace ava {
namespace platform {
namespace macos_ {

MacLibraryHandle::MacLibraryHandle(void* handle) : handle_(handle) {
}

MacLibraryHandle::~MacLibraryHandle() {
    if (handle_) {
        dlclose(handle_);
    }
}

void* MacLibraryHandle::ResolveSymbol(const std::string& symbol_name) {
    if (!handle_) return nullptr;
    return dlsym(handle_, symbol_name.c_str());
}

ILibraryHandle* MacLibraryLoader::Load(const std::string& library_name) {
    void* handle = dlopen(library_name.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) return nullptr;
    return new MacLibraryHandle(handle);
}

void MacLibraryLoader::Unload(ILibraryHandle* handle) {
    delete handle;
}

} // namespace macos_
} // namespace platform
} // namespace ava

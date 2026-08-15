#include "LinLibrary.h"
#include <dlfcn.h>

namespace ava {
namespace platform {
namespace linux_ {

LinLibraryHandle::LinLibraryHandle(void* handle) : handle_(handle) {
}

LinLibraryHandle::~LinLibraryHandle() {
    if (handle_) {
        dlclose(handle_);
    }
}

void* LinLibraryHandle::ResolveSymbol(const std::string& symbol_name) {
    if (!handle_) return nullptr;
    return dlsym(handle_, symbol_name.c_str());
}

ILibraryHandle* LinLibraryLoader::Load(const std::string& library_name) {
    void* handle = dlopen(library_name.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) return nullptr;
    return new LinLibraryHandle(handle);
}

void LinLibraryLoader::Unload(ILibraryHandle* handle) {
    delete handle;
}

} // namespace linux_
} // namespace platform
} // namespace ava

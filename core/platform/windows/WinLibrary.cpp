#include "WinLibrary.h"

namespace ava {
namespace platform {
namespace windows {

WinLibraryHandle::WinLibraryHandle(HMODULE module) : module_(module) {}

void* WinLibraryHandle::ResolveSymbol(const std::string& symbol_name) {
    if (module_ == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(module_, symbol_name.c_str()));
}

ILibraryHandle* WinLibraryLoader::Load(const std::string& library_name) {
    HMODULE module = LoadLibraryA(library_name.c_str());
    if (module == nullptr) {
        return nullptr;
    }
    return new WinLibraryHandle(module);
}

void WinLibraryLoader::Unload(ILibraryHandle* handle) {
    if (handle == nullptr) {
        return;
    }
    WinLibraryHandle* win_handle = static_cast<WinLibraryHandle*>(handle);
    if (win_handle->module() != nullptr) {
        FreeLibrary(win_handle->module());
    }
    delete win_handle;
}

} // namespace windows
} // namespace platform
} // namespace ava

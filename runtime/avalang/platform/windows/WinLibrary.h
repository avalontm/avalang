#ifndef AVA_PLATFORM_WIN_LIBRARY_H
#define AVA_PLATFORM_WIN_LIBRARY_H

#include "../interfaces/ILibrary.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// See WinThread.h for why these are undefined: Windows.h's ANSI/Unicode
// dispatch macros collide with virtual method names in this platform layer.
#undef DeleteFile
#undef CreateDirectory
#undef GetCurrentDirectory
#undef SetCurrentDirectory
#undef CreateMutex

namespace ava {
namespace platform {
namespace windows {

class WinLibraryHandle : public ILibraryHandle {
public:
    explicit WinLibraryHandle(HMODULE module);

    void* ResolveSymbol(const std::string& symbol_name) override;

    HMODULE module() const { return module_; }

private:
    HMODULE module_;
};

class WinLibraryLoader : public ILibraryLoader {
public:
    ILibraryHandle* Load(const std::string& library_name) override;
    void Unload(ILibraryHandle* handle) override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_LIBRARY_H

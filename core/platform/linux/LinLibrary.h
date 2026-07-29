#ifndef AVA_PLATFORM_LIN_LIBRARY_H
#define AVA_PLATFORM_LIN_LIBRARY_H

#include "../interfaces/ILibrary.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB. TODO: back with dlopen/dlsym/dlclose.
class LinLibraryHandle : public ILibraryHandle {
public:
    explicit LinLibraryHandle(void* handle);

    void* ResolveSymbol(const std::string& symbol_name) override;

    void* handle() const { return handle_; }

private:
    void* handle_;
};

class LinLibraryLoader : public ILibraryLoader {
public:
    ILibraryHandle* Load(const std::string& library_name) override;
    void Unload(ILibraryHandle* handle) override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_LIBRARY_H

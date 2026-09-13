#ifndef AVA_PLATFORM_MAC_LIBRARY_H
#define AVA_PLATFORM_MAC_LIBRARY_H

#include "../interfaces/ILibrary.h"

namespace ava {
namespace platform {
namespace macos_ {

class MacLibraryHandle : public ILibraryHandle {
public:
    explicit MacLibraryHandle(void* handle);
    ~MacLibraryHandle() override;

    void* ResolveSymbol(const std::string& symbol_name) override;

    void* handle() const { return handle_; }

private:
    void* handle_;
};

class MacLibraryLoader : public ILibraryLoader {
public:
    ILibraryHandle* Load(const std::string& library_name) override;
    void Unload(ILibraryHandle* handle) override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_LIBRARY_H

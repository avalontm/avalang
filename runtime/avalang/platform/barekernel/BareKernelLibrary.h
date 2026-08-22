#ifndef AVA_PLATFORM_BAREKERNEL_LIBRARY_H
#define AVA_PLATFORM_BAREKERNEL_LIBRARY_H

#include "../interfaces/ILibrary.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelLibraryHandle : public ILibraryHandle {
public:
    explicit BareKernelLibraryHandle(void* handle) : handle_(handle) {}
    ~BareKernelLibraryHandle() override;
    void* ResolveSymbol(const avastd::string& symbol_name) override;
private:
    void* handle_;
};

class BareKernelLibraryLoader : public ILibraryLoader {
public:
    ILibraryHandle* Load(const avastd::string& library_name) override;
    void Unload(ILibraryHandle* handle) override;
};

}
}
}

#endif

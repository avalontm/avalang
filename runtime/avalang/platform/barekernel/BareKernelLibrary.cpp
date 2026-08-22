#include "BareKernelLibrary.h"
#include "ckm_contract.h"

namespace ava {
namespace platform {
namespace barekernel {

BareKernelLibraryHandle::~BareKernelLibraryHandle() {
    if (handle_) ckm_dlclose(handle_);
}

void* BareKernelLibraryHandle::ResolveSymbol(const avastd::string& symbol_name) {
    if (!handle_) return nullptr;
    return ckm_dlsym(handle_, symbol_name.c_str());
}

ILibraryHandle* BareKernelLibraryLoader::Load(const avastd::string& library_name) {
    void* h = ckm_dlopen(library_name.c_str(), CKM_RTLD_NOW);
    if (!h) return nullptr;
    return new BareKernelLibraryHandle(h);
}

void BareKernelLibraryLoader::Unload(ILibraryHandle* handle) {
    delete handle;
}

}
}
}

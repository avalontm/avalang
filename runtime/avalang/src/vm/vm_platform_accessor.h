#ifndef AVA_VM_PLATFORM_ACCESSOR_H
#define AVA_VM_PLATFORM_ACCESSOR_H

#include "../platform/interfaces/IPlatform.h"

namespace ava {

// Thread-safe singleton accessor to the Platform Abstraction Layer.
// Provides VM, Runtime, and Compiler with a single entry point to OS services.
class VmPlatformAccessor {
public:
    // Returns the global IPlatform instance (lazy-initialized, thread-safe).
    static platform::IPlatform& Get();

private:
    VmPlatformAccessor() = delete;
    ~VmPlatformAccessor() = delete;
};

} // namespace ava

#endif // AVA_VM_PLATFORM_ACCESSOR_H

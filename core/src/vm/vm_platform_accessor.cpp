#include "vm_platform_accessor.h"
#include "../platform/Platform.h"

namespace ava {

platform::IPlatform& VmPlatformAccessor::Get() {
    // Meyer's Singleton: thread-safe initialization in C++11+
    // Static local variable is initialized once, on first call.
    // The compiler generates thread-safe initialization code automatically.
    static std::unique_ptr<platform::IPlatform> g_platform = platform::Platform::Create();
    return *g_platform;
}

} // namespace ava

#include "vm_platform_accessor.h"
#include "../platform/Platform.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

namespace {
avastd::mutex g_override_mutex;
avastd::unique_ptr<platform::IPlatform> g_override_platform;
} // namespace

platform::IPlatform& VmPlatformAccessor::Get() {
    // Meyer's Singleton: thread-safe initialization in C++11+
    // Static local variable is initialized once, on first call.
    // The compiler generates thread-safe initialization code automatically.
    static avastd::unique_ptr<platform::IPlatform> g_real_platform = platform::Platform::Create();

    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);
    if (g_override_platform) return *g_override_platform;
    return *g_real_platform;
}

void VmPlatformAccessor::SetOverride(avastd::unique_ptr<platform::IPlatform> platform) {
    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);
    g_override_platform = avastd::move(platform);
}

void VmPlatformAccessor::ClearOverride() {
    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);
    g_override_platform.reset();
}

} // namespace ava

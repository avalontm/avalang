#include "vm_platform_accessor.h"
#include "../platform/Platform.h"
#include <mutex>

namespace ava {

namespace {
std::mutex g_override_mutex;
std::unique_ptr<platform::IPlatform> g_override_platform;
} // namespace

platform::IPlatform& VmPlatformAccessor::Get() {
    // Meyer's Singleton: thread-safe initialization in C++11+
    // Static local variable is initialized once, on first call.
    // The compiler generates thread-safe initialization code automatically.
    static std::unique_ptr<platform::IPlatform> g_real_platform = platform::Platform::Create();

    std::lock_guard<std::mutex> lock(g_override_mutex);
    if (g_override_platform) return *g_override_platform;
    return *g_real_platform;
}

void VmPlatformAccessor::SetOverride(std::unique_ptr<platform::IPlatform> platform) {
    std::lock_guard<std::mutex> lock(g_override_mutex);
    g_override_platform = std::move(platform);
}

void VmPlatformAccessor::ClearOverride() {
    std::lock_guard<std::mutex> lock(g_override_mutex);
    g_override_platform.reset();
}

} // namespace ava

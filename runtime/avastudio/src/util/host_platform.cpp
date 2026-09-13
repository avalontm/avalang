#include "util/host_platform.h"

namespace studio::util {

HostPlatform DetectedHostPlatform() {
#if defined(_WIN32)
    return HostPlatform::kWindows;
#elif defined(__APPLE__)
    return HostPlatform::kMacOS;
#elif defined(__linux__)
    return HostPlatform::kLinux;
#else
    return HostPlatform::kUnknown;
#endif
}

std::string HostPlatformName(HostPlatform platform) {
    switch (platform) {
        case HostPlatform::kWindows: return "Windows";
        case HostPlatform::kMacOS: return "macOS";
        case HostPlatform::kLinux: return "Linux";
        case HostPlatform::kUnknown: return "Unknown";
    }
    return "Unknown";
}

std::string HostExecutableExtension(HostPlatform platform) {
    return platform == HostPlatform::kWindows ? ".exe" : "";
}

std::string HostLibraryExtension(HostPlatform platform) {
    switch (platform) {
        case HostPlatform::kWindows: return ".dll";
        case HostPlatform::kMacOS: return ".dylib";
        case HostPlatform::kLinux: return ".so";
        case HostPlatform::kUnknown: return "";
    }
    return "";
}

}

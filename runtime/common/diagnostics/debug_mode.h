#pragma once

#include <cstdlib>
#include <cstring>

namespace ava {
namespace diag {

namespace detail {

inline bool& DebugRuntimeState() {
    static bool enabled = false;
    return enabled;
}

inline bool EnvFlagIsSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

}  // namespace detail

inline void InitDebugRuntime(int& argc, char** argv) {
    bool requested = detail::EnvFlagIsSet("AVA_DEBUG");
    int kept = 1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug-runtime") == 0) {
            requested = true;
            continue;
        }
        argv[kept++] = argv[i];
    }
    if (kept < argc) {
        argv[kept] = nullptr;
        argc = kept;
    }
    detail::DebugRuntimeState() = requested;
}

inline bool DebugRuntimeEnabled() {
    return detail::DebugRuntimeState();
}

}  // namespace diag
}  // namespace ava

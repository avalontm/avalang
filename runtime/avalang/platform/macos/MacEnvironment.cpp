#include "MacEnvironment.h"

// STUB implementation.
// TODO(Phase 6): getenv/setenv/getcwd/chdir,
// mirroring core/platform/windows/MacEnvironment.cpp.

namespace ava {
namespace platform {
namespace macos_ {

bool MacEnvironment::GetEnvVar(const std::string& /*name*/, std::string& /*out_value*/) {
    return false;
}

bool MacEnvironment::SetEnvVar(const std::string& /*name*/, const std::string& /*value*/) {
    return false;
}

std::string MacEnvironment::GetCurrentDirectory() {
    return std::string();
}

bool MacEnvironment::SetCurrentDirectory(const std::string& /*path*/) {
    return false;
}

std::vector<std::string> MacEnvironment::GetCommandLineArgs() {
    return {};
}

} // namespace macos_
} // namespace platform
} // namespace ava

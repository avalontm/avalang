#include "LinEnvironment.h"

// STUB implementation.
// TODO(Phase 5): getenv/setenv/getcwd/chdir,
// mirroring core/platform/windows/LinEnvironment.cpp.

namespace ava {
namespace platform {
namespace linux_ {

bool LinEnvironment::GetEnvVar(const std::string& /*name*/, std::string& /*out_value*/) {
    return false;
}

bool LinEnvironment::SetEnvVar(const std::string& /*name*/, const std::string& /*value*/) {
    return false;
}

std::string LinEnvironment::GetCurrentDirectory() {
    return std::string();
}

bool LinEnvironment::SetCurrentDirectory(const std::string& /*path*/) {
    return false;
}

std::vector<std::string> LinEnvironment::GetCommandLineArgs() {
    return {};
}

} // namespace linux_
} // namespace platform
} // namespace ava

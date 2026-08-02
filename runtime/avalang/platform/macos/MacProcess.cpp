#include "MacProcess.h"

// STUB implementation -- never actually spawns a process.
// TODO(Phase 6): fork/exec/waitpid (Linux) or
// posix_spawn (macOS), mirroring core/platform/windows/MacProcess.cpp.

namespace ava {
namespace platform {
namespace macos_ {

uint64_t MacProcess::CurrentProcessId() const {
    return 0;
}

bool MacProcess::Execute(const std::string& /*command*/,
                            const std::vector<std::string>& /*args*/,
                            ProcessResult& /*out_result*/) {
    return false;
}

} // namespace macos_
} // namespace platform
} // namespace ava

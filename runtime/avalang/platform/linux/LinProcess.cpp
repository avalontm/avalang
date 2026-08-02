#include "LinProcess.h"

// STUB implementation -- never actually spawns a process.
// TODO(Phase 5): fork/exec/waitpid (Linux) or
// posix_spawn (macOS), mirroring core/platform/windows/LinProcess.cpp.

namespace ava {
namespace platform {
namespace linux_ {

uint64_t LinProcess::CurrentProcessId() const {
    return 0;
}

bool LinProcess::Execute(const std::string& /*command*/,
                            const std::vector<std::string>& /*args*/,
                            ProcessResult& /*out_result*/) {
    return false;
}

} // namespace linux_
} // namespace platform
} // namespace ava

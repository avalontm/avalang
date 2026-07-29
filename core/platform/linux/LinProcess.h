#ifndef AVA_PLATFORM_LIN_PROCESS_H
#define AVA_PLATFORM_LIN_PROCESS_H

#include "../interfaces/IProcess.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB. TODO: back with fork/exec/waitpid (Linux) or posix_spawn (macOS).
class LinProcess : public IProcess {
public:
    uint64_t CurrentProcessId() const override;

    bool Execute(const std::string& command,
                 const std::vector<std::string>& args,
                 ProcessResult& out_result) override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_PROCESS_H

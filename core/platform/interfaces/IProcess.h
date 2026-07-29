#ifndef AVA_PLATFORM_IPROCESS_H
#define AVA_PLATFORM_IPROCESS_H

#include <string>
#include <vector>
#include <cstdint>

namespace ava {
namespace platform {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
    std::string stderr_output;
};

class IProcess {
public:
    virtual ~IProcess() = default;

    virtual uint64_t CurrentProcessId() const = 0;

    // Runs `command` with `args`, blocks until it exits.
    virtual bool Execute(const std::string& command,
                          const std::vector<std::string>& args,
                          ProcessResult& out_result) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPROCESS_H

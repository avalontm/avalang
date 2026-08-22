#ifndef AVA_PLATFORM_IPROCESS_H
#define AVA_PLATFORM_IPROCESS_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IProcess.
#include "PAL_ABI.h"

#include "../barekernel/stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {

struct ProcessResult {
    int exit_code = -1;
    avastd::string stdout_output;
    avastd::string stderr_output;
};

class IProcess {
public:
    virtual ~IProcess() = default;

    virtual uint64_t CurrentProcessId() const = 0;

    // Runs `command` with `args`, blocks until it exits.
    virtual bool Execute(const avastd::string& command,
                          const avastd::vector<avastd::string>& args,
                          ProcessResult& out_result) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPROCESS_H

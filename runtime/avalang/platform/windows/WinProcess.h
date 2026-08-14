#ifndef AVA_PLATFORM_WIN_PROCESS_H
#define AVA_PLATFORM_WIN_PROCESS_H

#include "../interfaces/IProcess.h"
#include "../interfaces/IProcessStream.h"

namespace ava {
namespace platform {
namespace windows {

class WinProcess : public IProcess, public IProcessStream {
public:
    uint64_t CurrentProcessId() const override;

    bool Execute(const std::string& command,
                 const std::vector<std::string>& args,
                 ProcessResult& out_result) override;

    // See IProcessStream.h. Live-output twin of Execute() above -- same
    // child process setup, but reads/forwards stdout+stderr as they
    // arrive instead of only after the process exits.
    bool ExecuteStreaming(const std::string& command, const std::vector<std::string>& args,
                          const std::function<void(const std::string&)>& on_output,
                          int& out_exit_code) override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_PROCESS_H

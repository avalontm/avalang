#ifndef AVA_PLATFORM_WIN_PROCESS_H
#define AVA_PLATFORM_WIN_PROCESS_H

#include "../interfaces/IProcess.h"

namespace ava {
namespace platform {
namespace windows {

class WinProcess : public IProcess {
public:
    uint64_t CurrentProcessId() const override;

    bool Execute(const std::string& command,
                 const std::vector<std::string>& args,
                 ProcessResult& out_result) override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_PROCESS_H

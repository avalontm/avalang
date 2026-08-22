#ifndef AVA_PLATFORM_BAREKERNEL_PROCESS_H
#define AVA_PLATFORM_BAREKERNEL_PROCESS_H

#include "../interfaces/IProcess.h"
#include "../interfaces/IProcessStream.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelProcess : public IProcess
#if CKM_CAP_PROCESS_EXEC
                         , public IProcessStream
#endif
{
public:
    uint64_t CurrentProcessId() const override;
    bool Execute(const avastd::string& command,
                 const avastd::vector<avastd::string>& args,
                 ProcessResult& out_result) override;

#if CKM_CAP_PROCESS_EXEC
    bool ExecuteStreaming(const avastd::string& command,
                          const avastd::vector<avastd::string>& args,
                          const avastd::function<void(const avastd::string&)>& on_output,
                          int& out_exit_code) override;
#endif
};

}
}
}

#endif

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
    // No stdin pipe on this backend yet (BareKernel's Execute() below is
    // already a synchronous run-to-completion call with no live I/O of
    // any kind) -- `on_started`, if given, is simply never invoked, same
    // as IProcessStream.h documents for a backend that can't offer one.
    bool ExecuteStreaming(const avastd::string& command,
                          const avastd::vector<avastd::string>& args,
                          const avastd::function<void(const avastd::string&)>& on_output,
                          int& out_exit_code,
                          const avastd::function<void(avastd::shared_ptr<IStdinWriter>)>& on_started) override;
#endif
};

}
}
}

#endif

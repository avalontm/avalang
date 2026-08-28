#include "BareKernelProcess.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

uint64_t BareKernelProcess::CurrentProcessId() const {
    return static_cast<uint64_t>(ckm_getpid());
}

bool BareKernelProcess::Execute(const avastd::string& command,
                                 const avastd::vector<avastd::string>& args,
                                 ProcessResult& out_result) {
#if CKM_CAP_PROCESS_EXEC
    avastd::vector<const char*> argv;
    argv.push_back(command.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    int pid = ckm_spawn(command.c_str(), argv.data(), (int)argv.size() - 1);
    if (pid < 0) { out_result.exit_code = -1; return false; }

    int exit_code = -1;
    // sys_waitpid real (Fase 2) devuelve el PID reapeado (>0) en exito, no 0
    // -- antes con el stub ENOSYS (-38) "!= 0" acertaba por casualidad.
    if (ckm_waitpid(pid, &exit_code) < 0) { out_result.exit_code = -1; return false; }
    out_result.exit_code = exit_code;
    out_result.stdout_output.clear();
    out_result.stderr_output.clear();
    return true;
#else
    (void)command; (void)args;
    out_result.exit_code = -1;
    return false;
#endif
}

#if CKM_CAP_PROCESS_EXEC
bool BareKernelProcess::ExecuteStreaming(const avastd::string& command,
                                          const avastd::vector<avastd::string>& args,
                                          const avastd::function<void(const avastd::string&)>& on_output,
                                          int& out_exit_code,
                                          const avastd::function<void(avastd::shared_ptr<IStdinWriter>)>& on_started) {
    (void)on_started;  // no stdin pipe on this backend -- see the header comment
    ProcessResult r;
    bool ok = Execute(command, args, r);
    if (ok && !r.stdout_output.empty()) on_output(r.stdout_output);
    if (ok && !r.stderr_output.empty()) on_output(r.stderr_output);
    out_exit_code = r.exit_code;
    return ok;
}
#endif

}
}
}

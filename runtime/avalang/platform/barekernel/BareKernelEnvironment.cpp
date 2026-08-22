#include "BareKernelEnvironment.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"

namespace ava {
namespace platform {
namespace barekernel {

bool BareKernelEnvironment::GetEnvVar(const avastd::string& name, avastd::string& out_value) {
#if CKM_CAP_ENVVARS
    char buf[4096];
    int n = ckm_getenv(name.c_str(), buf, sizeof(buf));
    if (n <= 0) return false;
    out_value.assign(buf, static_cast<size_t>(n));
    return true;
#else
    (void)name; (void)out_value;
    return false;
#endif
}

bool BareKernelEnvironment::SetEnvVar(const avastd::string& name, const avastd::string& value) {
#if CKM_CAP_ENVVARS
    return ckm_setenv(name.c_str(), value.c_str()) == 0;
#else
    (void)name; (void)value;
    return false;
#endif
}

avastd::string BareKernelEnvironment::GetCurrentDirectory() {
    char buf[4096];
    if (ckm_getcwd(buf, sizeof(buf)) < 0) return avastd::string();
    return avastd::string(buf);
}

bool BareKernelEnvironment::SetCurrentDirectory(const avastd::string& path) {
    return ckm_chdir(path.c_str()) == 0;
}

avastd::vector<avastd::string> BareKernelEnvironment::GetCommandLineArgs() {
    // The kernel ABI does not currently expose argv at the CKM level -- the
    // caller is expected to wire its own loader that knows how the process
    // was started. Returning an empty vector matches the "no info available"
    // contract: code that needs argv should query a higher-level hook.
    return {};
}

}
}
}

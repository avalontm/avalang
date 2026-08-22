#ifndef AVA_PLATFORM_BAREKERNEL_ENVIRONMENT_H
#define AVA_PLATFORM_BAREKERNEL_ENVIRONMENT_H

#include "../interfaces/IEnvironment.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelEnvironment : public IEnvironment {
public:
    bool GetEnvVar(const avastd::string& name, avastd::string& out_value) override;
    bool SetEnvVar(const avastd::string& name, const avastd::string& value) override;
    avastd::string GetCurrentDirectory() override;
    bool SetCurrentDirectory(const avastd::string& path) override;
    avastd::vector<avastd::string> GetCommandLineArgs() override;
};

}
}
}

#endif

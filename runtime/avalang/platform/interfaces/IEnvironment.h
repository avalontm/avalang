#ifndef AVA_PLATFORM_IENVIRONMENT_H
#define AVA_PLATFORM_IENVIRONMENT_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IEnvironment.
#include "PAL_ABI.h"

#include "../barekernel/stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {

class IEnvironment {
public:
    virtual ~IEnvironment() = default;

    virtual bool GetEnvVar(const avastd::string& name, avastd::string& out_value) = 0;
    virtual bool SetEnvVar(const avastd::string& name, const avastd::string& value) = 0;

    virtual avastd::string GetCurrentDirectory() = 0;
    virtual bool SetCurrentDirectory(const avastd::string& path) = 0;

    virtual avastd::vector<avastd::string> GetCommandLineArgs() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IENVIRONMENT_H

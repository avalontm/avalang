#ifndef AVA_PLATFORM_IENVIRONMENT_H
#define AVA_PLATFORM_IENVIRONMENT_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IEnvironment.
#include "PAL_ABI.h"

#include <string>
#include <vector>

namespace ava {
namespace platform {

class IEnvironment {
public:
    virtual ~IEnvironment() = default;

    virtual bool GetEnvVar(const std::string& name, std::string& out_value) = 0;
    virtual bool SetEnvVar(const std::string& name, const std::string& value) = 0;

    virtual std::string GetCurrentDirectory() = 0;
    virtual bool SetCurrentDirectory(const std::string& path) = 0;

    virtual std::vector<std::string> GetCommandLineArgs() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IENVIRONMENT_H

#ifndef AVA_PLATFORM_IENVIRONMENT_H
#define AVA_PLATFORM_IENVIRONMENT_H

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

#ifndef AVA_PLATFORM_MAC_ENVIRONMENT_H
#define AVA_PLATFORM_MAC_ENVIRONMENT_H

#include "../interfaces/IEnvironment.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB. TODO: back with getenv/setenv/getcwd/chdir.
class MacEnvironment : public IEnvironment {
public:
    bool GetEnvVar(const std::string& name, std::string& out_value) override;
    bool SetEnvVar(const std::string& name, const std::string& value) override;

    std::string GetCurrentDirectory() override;
    bool SetCurrentDirectory(const std::string& path) override;

    std::vector<std::string> GetCommandLineArgs() override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_ENVIRONMENT_H

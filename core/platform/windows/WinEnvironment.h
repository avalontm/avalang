#ifndef AVA_PLATFORM_WIN_ENVIRONMENT_H
#define AVA_PLATFORM_WIN_ENVIRONMENT_H

#include "../interfaces/IEnvironment.h"

namespace ava {
namespace platform {
namespace windows {

class WinEnvironment : public IEnvironment {
public:
    bool GetEnvVar(const std::string& name, std::string& out_value) override;
    bool SetEnvVar(const std::string& name, const std::string& value) override;

    std::string GetCurrentDirectory() override;
    bool SetCurrentDirectory(const std::string& path) override;

    std::vector<std::string> GetCommandLineArgs() override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_ENVIRONMENT_H

#ifndef AVA_PLATFORM_MAC_ENVIRONMENT_H
#define AVA_PLATFORM_MAC_ENVIRONMENT_H

#include "../interfaces/IEnvironment.h"

namespace ava {
namespace platform {
namespace macos_ {

class MacEnvironment : public IEnvironment {
public:
    bool GetEnvVar(const std::string& name, std::string& out_value) override;
    bool SetEnvVar(const std::string& name, const std::string& value) override;

    std::string GetCurrentDirectory() override;
    bool SetCurrentDirectory(const std::string& path) override;

    std::vector<std::string> GetCommandLineArgs() override;
};

// Debe llamarse una vez desde main() en macOS (con el argc/argv reales
// del proceso) antes de que cualquier script use
// System.Environment.GetCommandLineArgs() -- mismo esquema que
// linux_::SetCommandLineArgs, ver LinEnvironment.h para el detalle de
// por que hace falta (no hay forma de recuperar argv despues de que
// main() ya arranco).
void SetCommandLineArgs(int argc, char** argv);

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_ENVIRONMENT_H

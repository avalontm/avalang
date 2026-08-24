#ifndef AVA_PLATFORM_LIN_ENVIRONMENT_H
#define AVA_PLATFORM_LIN_ENVIRONMENT_H

#include "../interfaces/IEnvironment.h"

namespace ava {
namespace platform {
namespace linux_ {

class LinEnvironment : public IEnvironment {
public:
    bool GetEnvVar(const std::string& name, std::string& out_value) override;
    bool SetEnvVar(const std::string& name, const std::string& value) override;

    std::string GetCurrentDirectory() override;
    bool SetCurrentDirectory(const std::string& path) override;

    std::vector<std::string> GetCommandLineArgs() override;
};

// Debe llamarse una vez desde main() en Linux (con el argc/argv reales
// del proceso) antes de que cualquier script use
// System.Environment.GetCommandLineArgs() -- a diferencia de Windows
// (que reconstruye argv en cada llamada via CommandLineToArgvW), acá
// no hay forma de recuperar argv despues de que main() ya arrancó, asi
// que si nadie llama a esto GetCommandLineArgs() devuelve siempre una
// lista vacia. Encontrado como gap real en la Fase 7 de
// AVALANG_IMPORT_SYSTEM_PLAN.md: la funcion ya existia en
// LinEnvironment.cpp pero no estaba declarada acá y ava_cli no la
// llamaba.
void SetCommandLineArgs(int argc, char** argv);

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_ENVIRONMENT_H

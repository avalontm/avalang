#include <cstdlib>
#include <iostream>
#include <string>

#include <windows.h>

#include "native/native_app_host.h"
#include "platform/windows/WinBackendEntry.h"

namespace {

std::string ExtractFlagValue(int& argc, char** argv, const char* flagName) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == flagName) {
            if (i + 1 >= argc) return "";
            std::string value = argv[i + 1];
            for (int j = i; j + 2 < argc; ++j) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            return value;
        }
    }
    return "";
}

std::string EnvOrDefault(const char* varName) {
    const char* value = std::getenv(varName);
    return value ? std::string(value) : "";
}

}  // namespace

int main(int argc, char** argv) {
    avalang::ui::platform::windows::LinkBackend();

    std::string entryFile = ExtractFlagValue(argc, argv, "--entry");
    if (entryFile.empty()) entryFile = EnvOrDefault("AVA_ENTRY_FILE");
    if (entryFile.empty()) entryFile = "main.ava";

    std::string widthFlag = ExtractFlagValue(argc, argv, "--width");
    std::string heightFlag = ExtractFlagValue(argc, argv, "--height");
    int width = widthFlag.empty() ? 1024 : std::stoi(widthFlag);
    int height = heightFlag.empty() ? 720 : std::stoi(heightFlag);

    std::string projectDir = argc > 1 ? argv[1] : ".";

    std::string error;
    int code = avahost::native::RunNativeApp(projectDir, entryFile, width, height, error);
    if (code != 0 && !error.empty()) {
        // avanative corre bajo subsystem WINDOWS (sin consola asignada,
        // ver CMakeLists.txt), asi que std::cerr no lo ve nadie -- un
        // MessageBox es la unica forma de que un error temprano (p.ej.
        // entry file no encontrado) no sea un cierre silencioso.
        std::string message = "avanative: " + error;
        MessageBoxA(nullptr, message.c_str(), "Avalang", MB_OK | MB_ICONERROR);
    }
    return code;
}

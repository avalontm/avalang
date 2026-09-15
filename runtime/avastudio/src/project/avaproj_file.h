#pragma once

#include <optional>
#include <string>
#include <vector>

namespace studio {

enum class AvaProjTarget { kDesktop, kBareKernel };
enum class AvaProjOutputType { kExe, kLibrary };

std::string TargetToString(AvaProjTarget target);

AvaProjTarget TargetFromString(const std::string& value);

std::string OutputTypeToString(AvaProjOutputType type);

AvaProjOutputType OutputTypeFromString(const std::string& value);

struct AvaProjReference {
    std::string include;
};

struct AvaProjFile {
    std::string project_name;
    std::string entry_file = "main.ava";
    AvaProjTarget target = AvaProjTarget::kDesktop;
    AvaProjOutputType output_type = AvaProjOutputType::kExe;
    std::string modules_path;
    std::string icon;
    std::string out_dir = "bin";

    bool obfuscate = false;
    bool obfuscate_strings = false;
    bool flatten_control_flow = false;
    bool zero_disk = false;
    bool debug_unencrypted = false;
    // No expuesto como checkbox en Project Properties -- se edita a mano en
    // el .avaproj (<UsesUi>true</UsesUi> bajo el PropertyGroup Label="Build")
    // para proyectos que importan componentes .avaui. Controla si el build
    // (ava_cli build, ver build_panel.cpp) pasa --with-ui -- sin esto,
    // AVA_BUILD_UI es OFF por default y el binario no depende de
    // avalang_ui.dll/.so.
    bool uses_ui = false;

    std::vector<AvaProjReference> references;
};

std::optional<AvaProjFile> LoadAvaProjFile(const std::string& path);

bool SaveAvaProjFile(const std::string& path, const AvaProjFile& data);

std::string FindAvaProjInDir(const std::string& dir);

// Fix: variante que devuelve TODOS los .avaproj encontrados en el nivel
// superior de `dir`, en vez de colapsar el caso "hay mas de uno" a "".
// FindAvaProjInDir (arriba) se reimplementa en base a esta y sigue
// devolviendo "" cuando hay 0 o >1 -- para no romper a quien ya la use
// esperando "un solo candidato o nada" -- pero ahora LoadProjectConfig
// (project_config.cpp) puede distinguir "no hay proyecto todavia" de
// "hay varios y no se cual queres" y avisar en vez de inventar un
// proyecto default silenciosamente.
std::vector<std::string> FindAllAvaProjInDir(const std::string& dir);

}

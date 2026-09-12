#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace studio {

struct ExplorerState {
    std::string root_dir;

    std::string selected_path;

    // Nombres de archivo/carpeta (comparacion exacta, sin importar en que
    // nivel del arbol aparezcan) que el explorador no debe listar. Pensado
    // para carpetas generadas como la de salida del build ("bin" por
    // default, ver AvaProjFile::out_dir) que no aportan nada al desarrollo
    // y solo ensucian el arbol -- pero es generico, no esta atado a "bin".
    std::vector<std::string> excluded_names;
};

struct ExplorerResult {

    std::optional<std::string> file_to_open;

    std::optional<std::string> file_deleted;

    std::optional<std::pair<std::string, std::string>> file_renamed;

    std::optional<std::string> reveal_in_file_manager;

    bool open_project_properties = false;
};

ExplorerResult DrawExplorerPanel(ExplorerState& state, bool* p_open = nullptr);

}
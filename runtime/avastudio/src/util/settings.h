#pragma once

#include <string>
#include <vector>

namespace studio {

struct StudioSettings {
    std::string language;

    std::string modules_path;

    // Fix: recordar el ultimo proyecto abierto (explorer_state.root_dir) para
    // restaurarlo al iniciar Ava Studio. Sin esto, cada arranque que no venga
    // de "abrir con" un .avaproj (ver ParseProjectArgPath en main.cpp) volvia
    // siempre a ResolveWorkspaceDir() (<exe_dir>/scripts), perdiendo de vista
    // en que proyecto estaba trabajando el usuario -- si ese workspace tenia
    // varias subcarpetas de proyecto, Studio no tenia forma de saber cual
    // era la activa.
    std::string last_project_dir;

    std::vector<std::string> disabled_plugins;

    std::vector<std::string> closed_panels;
};

StudioSettings LoadSettings();

void SaveSettings(const StudioSettings& settings);

}

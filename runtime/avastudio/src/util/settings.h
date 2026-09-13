#pragma once

#include <string>
#include <vector>

namespace studio {

struct StudioSettings {
    std::string language;

    std::string modules_path;

    // Minimap en el editor de código: panel angosto a la derecha con una
    // vista miniatura coloreada de todo el archivo, click/drag para saltar,
    // y un rectángulo que marca el viewport visible -- dibujado a mano en
    // DrawEditorMinimap (editor_panel.cpp), porque la librería base
    // (ImGuiColorTextEdit, rama "Legacy") no trae un minimapa de panel
    // lateral, solo una franja delgada dentro de la scrollbar
    // (showScrollbarMiniMap) que se sigue usando como fallback cuando este
    // está apagado. Ver InitTab en editor_panel.cpp para donde se aplica a
    // cada tab.
    bool show_minimap = true;

    bool show_inlay_hints = true;

    bool format_on_save = false;

    bool format_on_type = false;

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

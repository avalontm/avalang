// Fase 0's acceptance-criterion plugin: a "hola mundo" panel with a
// button that prints something into the Output panel, loaded
// dynamically as its own .dll/.so -- none of this file's code is
// compiled into ava_studio.exe (see runtime/avastudio/CMakeLists.txt's
// add_subdirectory(plugins/hello_world), which builds this as a
// separate shared library and copies it into ava_studio's own
// plugins/ folder as a POST_BUILD step).
//
// This is also meant to double as the minimal reference example for
// anyone writing a real plugin later (the ai_agent from Fase 1
// onwards): the only header it includes is plugin_api.h -- no ImGui,
// no avastudio/ headers, nothing else from this repo. That's the
// whole point of the ABI boundary (see plugin_api.h's header comment).

#include "plugin_api.h"

#include <string>

namespace {

struct HelloWorldState {
    char name_buffer[128] = "world";
    int click_count = 0;
};

HelloWorldState g_state;

// Fase 0 only hands the host struct to ava_plugin_init(), not to
// draw() -- a plugin that needs it later (every real plugin will, to
// call host->ui.*/services.* from inside its draw callback) stashes
// the pointer itself, exactly like this global. It stays valid from
// ava_plugin_init() until ava_plugin_shutdown() returns (see
// plugin_api.h's contract on AvaStudioHost's lifetime).
AvaStudioHost* g_host = nullptr;

void DrawHelloPanel(AvaPanelContext* ctx, void* user_data) {
    auto* state = static_cast<HelloWorldState*>(user_data);
    if (!g_host) return;

    g_host->ui.text_wrapped(ctx, "Hola desde un plugin cargado dinamicamente.");
    g_host->ui.spacing(ctx);
    g_host->ui.input_text(ctx, "Nombre", state->name_buffer, sizeof(state->name_buffer));
    g_host->ui.same_line(ctx);
    if (g_host->ui.button(ctx, "Saludar")) {
        state->click_count++;
        std::string message = "Hola, " + std::string(state->name_buffer) + "! (click #" +
                               std::to_string(state->click_count) + ")";
        g_host->services.log(g_host, message.c_str());
    }
    g_host->ui.separator(ctx);

    const char* project_root = g_host->services.get_project_root(g_host);
    std::string root_line = "Proyecto actual: ";
    root_line += (project_root && project_root[0] != '\0') ? project_root : "(ninguno)";
    g_host->ui.text_wrapped(ctx, root_line.c_str());
}

} // namespace

extern "C" int ava_plugin_abi_version() {
    return AVA_STUDIO_PLUGIN_ABI_VERSION;
}

extern "C" bool ava_plugin_init(AvaStudioHost* host) {
    if (!host) return false;
    g_host = host;

    AvaPanelRegistration registration{};
    registration.name = "Hello World Plugin";
    registration.draw = &DrawHelloPanel;
    registration.user_data = &g_state;
    registration.default_dock_slot = AVA_DOCK_BOTTOM;

    const int panel_id = host->register_panel(host, &registration);
    if (panel_id < 0) return false;

    host->services.log(host, "hello_world plugin initialized");
    return true;
}

extern "C" void ava_plugin_shutdown() {
    g_host = nullptr;
}

// Fase 9: optional metadata, shown in the "Plugins" menu. None of
// these three are required -- a plugin that skips all of them still
// loads exactly as before, the menu just shows nothing extra under its
// checkbox. Included here anyway since this file doubles as the
// reference example for anyone writing a real plugin.
extern "C" const char* ava_plugin_display_name() {
    return "Hello World Plugin";
}

extern "C" const char* ava_plugin_version() {
    return "1.0.0";
}

extern "C" const char* ava_plugin_author() {
    return "Ava Studio";
}

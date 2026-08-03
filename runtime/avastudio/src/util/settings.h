#pragma once

#include <string>
#include <vector>

namespace studio {

// User-configurable, persisted-across-sessions settings for Ava Studio.
// Grow this struct as more Settings-dialog options show up (see
// panels/titlebar_panel.cpp's Properties modal).
struct StudioSettings {
    // Base modules folder passed to ava_vm_set_stdlib_path -- see the
    // module-resolution order comment in public/include/avalang.h.
    // Empty ("") is the default, both on a first run and whenever the
    // user clears the Properties field -- it means "use the modules/
    // folder next to the executable" (see
    // util::ResolveDefaultModulesDir()), resolved at the point of use
    // rather than baked into settings.ini, so the settings file stays
    // portable across machines/install locations. A non-empty value is
    // an explicit user override and is used as-is.
    std::string modules_path;

    // File names (e.g. "ai_agent.dll", "hello_world.so" -- same string
    // as PluginHost::PluginInfo::file_name) of plugins the user turned
    // off from the "Plugins" menu (see titlebar_panel.h). A file NOT in
    // this list is enabled -- that way a plugin dropped into plugins/
    // for the first time defaults to on, instead of every existing
    // settings.ini needing to know about it in advance. main.cpp passes
    // this straight into PluginHost::LoadAll/Reload.
    std::vector<std::string> disabled_plugins;

    // Names of panels the user closed -- either a plugin panel
    // (RegisteredPanel::name, e.g. "AI Agent") via its own tab X, the
    // "Plugins" modal's panel list, or the "View" menu, OR a built-in
    // panel (see panels/builtin_panels.h, e.g. "Explorer") via its tab X
    // or the "View" menu. A name NOT in this list is open -- same
    // "absence means default" convention as disabled_plugins, so a
    // panel a plugin registers for the first time defaults to visible.
    // Distinct from disabled_plugins: closing a panel just hides its
    // tab, it doesn't unload the plugin (AvaHostServices calls the
    // plugin makes in the background, e.g. apply_edit proposals, keep
    // working). main.cpp is what actually reads/writes this map at
    // runtime (see `panel_open` there); this is only the persisted form.
    std::vector<std::string> closed_panels;
};

// Loads persisted settings from the per-user config folder (same place
// as imgui.ini -- %APPDATA%/AvaStudio on Windows, ~/.config/AvaStudio
// elsewhere). Missing file or missing keys fall back to defaults, so
// this is always safe to call on startup even on a first run.
StudioSettings LoadSettings();

// Persists `settings` to that same per-user config folder. Best-effort:
// silently does nothing if the folder can't be created/written (e.g. a
// locked-down install), same philosophy as imgui.ini's own save.
void SaveSettings(const StudioSettings& settings);

} // namespace studio

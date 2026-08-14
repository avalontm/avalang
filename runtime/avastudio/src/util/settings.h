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

    // --- "Build" panel (see panels/build_panel.h) --------------------
    // Packages the current project into a distributable .exe by
    // shelling out to `ava_cli build` (runtime/avacli), which drives
    // runtime/avapack. Persisted so re-opening Ava Studio doesn't need
    // every field re-entered -- most only need to be set once per
    // machine/checkout. Empty string means "not set yet / auto-detect",
    // documented per-field in build_panel.cpp.

    std::string build_project_dir;   // --project. "" = use the Explorer panel's open folder.
    std::string build_entry_file;    // --entry, relative to build_project_dir. "" = auto-detect main.ava.
    std::string build_out_dir;       // --out (directory mode). "" = <project>/dist.
    std::string build_repo_root;     // --repo-root, the AvaLang repo checkout. "" = auto-detect.
    std::string build_ava_cli_path;  // Path to ava_cli(.exe). "" = auto-detect next to ava_studio.exe.
    std::string build_key_file;      // --key-file (optional, 32 raw AES-256 bytes). "" = random key per build.
    std::string build_vcpkg_root;    // VCPKG_ROOT env var for ava_cli's cmake configure step (see
                                      // install.bat). "" = use the VCPKG_ROOT already in the
                                      // environment, else auto-detect <repo_root>/vcpkg.

    bool build_obfuscate = false;             // --obfuscate
    bool build_obfuscate_strings = false;     // --obfuscate-strings (requires build_obfuscate)
    bool build_flatten_control_flow = false;  // --flatten-control-flow (requires build_obfuscate)
    bool build_zero_disk = false;             // --zero-disk
    bool build_debug_unencrypted = false;     // --debug (NOT for distribution, see avapack/README.md)
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

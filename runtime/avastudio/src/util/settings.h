#pragma once

#include <string>

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

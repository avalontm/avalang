#pragma once

#include <string>
#include <vector>

#include "plugins/plugin_host.h"
#include "util/settings.h"

namespace studio {

// The "Settings" panel (see PLAN_settings_panel.md, Fase 2) -- a normal
// dockable tab, same family as Explorer/Properties/Preview/Terminal/
// Output (see builtin_panels.h), that internally lays itself out as a
// VSCode-style master/detail split: a left sidebar listing sections
// grouped by header ("General", "Plugins") and a right content area
// showing whatever section is currently selected. This is a manual
// ImGui::BeginChild split *inside* this one tab -- it has nothing to do
// with the outer ImGui dockspace/tabs main.cpp otherwise manages.
//
// `settings`: the same StudioSettings instance main.cpp already owns
// and persists (previously fed only into the Properties modal, see
// titlebar_panel.cpp) -- mutated in place as fields are edited.
//
// `settings_panels`: this frame's plugin_host.SettingsPanels() -- every
// RegisteredPanel a loaded plugin registered with is_settings == true
// (see plugin_api.h/plugin_host.h Fase 1). One "Plugins" sidebar item
// per entry, drawn via panel.draw(ctx, panel.user_data) in the content
// area when selected -- same AvaPanelContext mechanism main.cpp's own
// panel-drawing loop already uses for normal plugin tabs (see
// plugin_ui_bridge.h), just without an ImGui::Begin/End of its own
// since we're already inside this panel's window.
//
// `out_settings_dirty`: set to true the one frame the "General" section's
// modules_path field is actually saved (Save button clicked) -- mirrors
// TitleBarResult::modules_save_requested's role in the old Properties
// modal. main.cpp is what actually calls studio::SaveSettings() when
// this comes back true; this function only mutates `settings` in
// memory and flags that a persist is due.
//
// `out_browse_requested`: set to true the one frame the modules_path
// row's "Browse..." button is clicked -- same role as
// TitleBarResult::modules_browse_requested (main.cpp owns the native
// folder-picker dialog; this panel has no OS integration of its own).
//
// `browsed_folder`: normally "". The one frame after the person picks a
// folder from that native dialog, main.cpp passes the chosen path back
// in here so this panel can drop it into its own modules_path text
// buffer -- same round-trip DrawTitleBar's own `browsed_folder` param
// already does for the (now-removed) Properties modal.
//
// `p_open`: same ImGui::Begin p_open convention as every other panel
// here (see properties_panel.h's DrawPropertiesPanel) -- pass the
// address of panel_open["Settings"] so the tab gets a close ("x")
// button. nullptr draws with no close button.
void DrawSettingsPanel(StudioSettings& settings, const std::vector<RegisteredPanel>& settings_panels,
                        bool& out_settings_dirty, bool& out_browse_requested, const std::string& browsed_folder,
                        bool* p_open = nullptr);

} // namespace studio

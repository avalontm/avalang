#pragma once

#include <string>
#include <vector>

namespace studio {

struct EditorState; // defined in panels/editor_panel.h
struct StudioSettings; // defined in util/settings.h
struct PluginInfo; // defined in plugins/plugin_host.h
struct RegisteredPanel; // defined in plugins/plugin_host.h

// Bounding box of a drawn UI element in screen coordinates -- the same
// space as ImGui::GetItemRectMin/Max() on the main viewport. main.cpp
// feeds these into platform/win32_titlebar.h so OS-level window dragging
// (WM_NCHITTEST -> HTCAPTION) doesn't swallow clicks meant for the
// buttons instead.
struct ScreenRect {
    float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
};

struct TitleBarResult {
    bool minimize_clicked = false;
    bool maximize_or_restore_clicked = false;
    bool close_clicked = false;
    ScreenRect minimize_rect;
    ScreenRect maximize_rect;
    ScreenRect close_rect;
    // File/Run menu buttons also need to be registered as real (HTCLIENT)
    // hit regions with platform/win32_titlebar.h, or Windows treats
    // clicking them as "drag the titlebar" and the click never reaches
    // ImGui -- see the comment above UpdateHitRegions().
    ScreenRect file_menu_rect;
    ScreenRect view_menu_rect;
    ScreenRect run_menu_rect;
    ScreenRect about_rect;
    // True while the File, View, or Run dropdown is open. Windows swallows clicks
    // on the parts of the titlebar strip that aren't registered as extra
    // hit regions (it treats them as HTCAPTION / "drag the window" before
    // the click ever reaches ImGui), so a click meant to dismiss an open
    // dropdown by clicking elsewhere on the titlebar never arrived. main.cpp
    // uses this flag to temporarily open up the whole strip as a real
    // (HTCLIENT) region while a dropdown is open, so ImGui's normal
    // click-outside-closes-popup behavior can actually see the click.
    bool any_popup_open = false;
    // File menu actions that need main.cpp (native dialogs / window
    // close) -- Save stays as editor_state.save_requested like before,
    // since it doesn't need anything outside the editor state.
    bool new_requested = false;
    bool open_requested = false;
    bool open_folder_requested = false;
    bool save_as_requested = false;
    bool quit_requested = false;

    // "File > Preferences > Settings" (Ctrl+,). main.cpp reacts by
    // ensuring panel_open["Settings"] = true and focusing that tab, same
    // pattern as reopening a panel toggled from View -- see main.cpp.
    bool open_settings_requested = false;

    // "Run > Build Executable..." (Ctrl+B), see panels/build_panel.h.
    // main.cpp reacts the exact same way as open_settings_requested,
    // just against panel_open["Build"] instead.
    bool build_requested = false;

    // "Plugins" menu (see the `plugins` param below). Set to the
    // file_name of whichever PluginInfo checkbox the user clicked this
    // frame, "" otherwise -- main.cpp is what actually flips it in
    // StudioSettings::disabled_plugins, persists that, and calls
    // PluginHost::Reload(), since this function has no access to
    // PluginHost or the settings file itself.
    std::string plugin_toggle_requested;

    // Per-panel visibility toggle -- set by either the "Plugins" modal's
    // panel list (see the `panels` param below) or the "View" menu's
    // checkboxes, whichever the user clicked this frame ("" otherwise).
    // Covers BOTH plugin panels (by RegisteredPanel::name) and built-in
    // ones (by the literal names in panels/builtin_panels.h -- "Explorer",
    // "Properties", etc.), since both are just entries in the same
    // StudioSettings::closed_panels list and the same runtime
    // `panel_open` map in main.cpp. main.cpp flips it in
    // closed_panels, persists that, and syncs `panel_open` -- this
    // function has no access to that map itself, same reasoning as
    // plugin_toggle_requested above.
    std::string panel_toggle_requested;
};

// Draws Ava Studio's VSCode-style title bar: brand mark + name, the
// File/Run menu, the active file name centered (like VSCode's window
// title), and flat minimize/maximize/close buttons on the right.
// `is_maximized` swaps the maximize icon for the "restore" icon, and
// `height` is both the drawn height and the caller's cue for how much
// vertical space to reserve above the dockspace. Save/Run menu clicks
// are applied directly onto `editor_state` (same as the old in-dockspace
// menu bar did) -- only the window-chrome buttons come back through the
// return value, since only main.cpp knows how to talk to GLFW/the OS.
//
// `plugins`: the current PluginHost::ScanAvailable() snapshot, used
// only to draw the "Plugins" menu's checkboxes (name, enabled/disabled,
// currently loaded or not) -- this function never loads/unloads
// anything itself, see TitleBarResult::plugin_toggle_requested.
//
// `panels`: the current PluginHost::Panels() snapshot -- every panel a
// loaded plugin has registered, regardless of whether the user closed
// its tab. Drawn as a second checkbox list in the same "Plugins" modal
// (checked = tab currently visible) so closing a panel from its own tab
// X still has an easy way back, without hunting for which plugin owns
// it -- see TitleBarResult::panel_toggle_requested. `closed_panels` is
// StudioSettings::closed_panels, read here only to know which
// checkboxes start unchecked.
//
// The "View" menu (see kBuiltinPanelNames in panels/builtin_panels.h)
// reads that same `closed_panels` list to draw a checkbox for every
// built-in panel (Explorer, Properties, Preview, Terminal, Output),
// followed by a checkbox for every plugin panel in `panels` -- checked
// when currently visible, unchecked when closed. Both sections funnel
// into the same TitleBarResult::panel_toggle_requested field.
TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized,
                             float height, const std::vector<PluginInfo>& plugins,
                             const std::vector<RegisteredPanel>& panels, const std::vector<std::string>& closed_panels);

} // namespace studio

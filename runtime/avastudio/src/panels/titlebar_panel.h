#pragma once

#include <string>

namespace studio {

struct EditorState; // defined in panels/editor_panel.h
struct StudioSettings; // defined in util/settings.h

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
    ScreenRect run_menu_rect;
    ScreenRect about_rect;
    // True while the File or Run dropdown is open. Windows swallows clicks
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

    // Properties dialog ("File > Properties"), modules-folder setting.
    // modules_browse_requested: user clicked "Browse..." -- main.cpp owns
    // the GLFWwindow* needed for the native folder picker, so it handles
    // this the same way it already does for open_folder_requested.
    // modules_save_requested: user clicked "Save" -- by then
    // `settings.modules_path` (passed into DrawTitleBar by reference) has
    // already been updated with the dialog's text field, so main.cpp just
    // needs to apply it (EngineBridge::SetModulesPath) and persist it
    // (util::SaveSettings).
    bool modules_browse_requested = false;
    bool modules_save_requested = false;
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
// `settings.modules_path` is read to pre-fill the Properties dialog's text
// field, and written to directly when the user edits it or clicks
// "Save" (see TitleBarResult::modules_save_requested) -- main.cpp is
// what actually applies/persists it, this function only edits the struct.
//
// `browsed_folder`: normally "". The one frame after the user clicks
// "Browse..." and picks a folder, main.cpp passes that folder here so
// this function can drop it into the dialog's text field -- the native
// folder picker itself has to run in main.cpp (needs the GLFWwindow*),
// so this is how its result gets back into the dialog a frame later.
TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized,
                             float height, const std::string& browsed_folder);

} // namespace studio

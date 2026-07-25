#pragma once

namespace studio {

struct EditorState; // defined in panels/editor_panel.h

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
TitleBarResult DrawTitleBar(EditorState& editor_state, bool is_maximized, float height);

} // namespace studio

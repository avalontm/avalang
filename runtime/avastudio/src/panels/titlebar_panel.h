#pragma once

#include <string>
#include <vector>

namespace studio {

struct EditorState;
struct StudioSettings;
struct PluginInfo;

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

    ScreenRect file_menu_rect;
    ScreenRect edit_menu_rect;
    ScreenRect view_menu_rect;
    ScreenRect run_menu_rect;
    ScreenRect about_rect;

    bool any_popup_open = false;

    bool new_requested = false;

    // Fase 6: "New Project..." lives above "New File" in the File menu --
    // it's the entry point for a folder that doesn't exist yet, so it
    // belongs with Open/Open Folder rather than being confused with
    // new_requested (a new *tab*, no project involved). Same edge-
    // triggered shape as the rest of TitleBarResult: main.cpp only calls
    // OpenNewProjectDialog() on the frame this is true.
    bool new_project_requested = false;

    bool open_requested = false;
    bool open_folder_requested = false;
    bool save_as_requested = false;
    bool quit_requested = false;

    bool open_settings_requested = false;

    bool build_requested = false;

    // Fase 3: "Command Palette..." lives at the top of the View menu --
    // it's an overlay/modal, not a dockable panel, so it doesn't belong in
    // kBuiltinPanelNames (same reasoning already noted in the plan for why
    // it's not a panel). Fase 7 removed the per-panel toggle list that used
    // to sit below it in this same menu (moved to the Activity Bar +
    // already-existing Command Palette entries), so this item is the only
    // thing left in the View menu now. main.cpp treats this the same
    // edge-triggered way as the other *_requested fields here:
    // OpenCommandPalette() only gets called on the frame this is true.
    bool command_palette_requested = false;

    // Fase 4: "Quick Open..." lives in the Edit menu next to
    // "Find in Project" -- same overlay/modal reasoning as
    // command_palette_requested above (it's not a dockable panel, so it
    // doesn't belong in kBuiltinPanelNames either). Edge-triggered the same
    // way: OpenQuickOpen() only gets called on the frame this is true.
    bool quick_open_requested = false;

    std::string plugin_toggle_requested;
};

// Fase 7: the per-panel toggle list that used to live in the View menu
// (looping kBuiltinPanelNames + the plugin panel list) moved to the new
// Activity Bar (for the common panels) and was already duplicated in the
// Command Palette (for every panel, Fase 3) -- so DrawTitleBar no longer
// needs the panel/closed_panels lists it used to build that menu from.
//
// `open_extensions_requested`: lets a caller outside this file (the new
// Activity Bar) open the Extensions modal without duplicating it -- the
// modal itself stays a static-bool local to titlebar_panel.cpp (same place
// the About modal already lives), this just gives it a second door in
// besides the File > Preferences > Extensions menu item.
TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized, float height,
                             const std::vector<PluginInfo>& plugins, bool open_extensions_requested = false);

}

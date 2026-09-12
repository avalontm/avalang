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

    bool new_project_requested = false;

    bool open_requested = false;
    bool open_folder_requested = false;
    bool save_as_requested = false;
    bool quit_requested = false;

    bool open_settings_requested = false;

    bool project_properties_requested = false;

    bool build_requested = false;

    bool command_palette_requested = false;

    bool quick_open_requested = false;

    std::string plugin_toggle_requested;
};

TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized, float height,
                             const std::vector<PluginInfo>& plugins, bool open_extensions_requested = false);

}
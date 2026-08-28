#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "design/design_document.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "parser/AvauiWriter.h"
#include "engine/engine_bridge.h"
#include "fonts/embedded_font.h"
#include "panels/activity_bar_panel.h"
#include "panels/builtin_panels.h"
#include "panels/command_palette.h"
#include "panels/editor_panel.h"
#include "panels/explorer_panel.h"
#include "panels/find_in_project_panel.h"
#include "panels/logs_panel.h"
#include "panels/new_project_panel.h"
#include "panels/pending_edits_panel.h"
#include "panels/preview_panel.h"
#include "panels/problems_panel.h"
#include "panels/properties_panel.h"
#include "panels/quick_open_panel.h"
#include "panels/build_panel.h"
#include "panels/settings_panel.h"
#include "panels/terminal_panel.h"
#include "panels/titlebar_panel.h"
#include "panels/toolbox_panel.h"
#include "palette.h"
#include "plugins/plugin_host.h"
#include "plugins/plugin_ui_bridge.h"
#include "platform/win32_titlebar.h"
#include "theme.h"
#include "util/ava_cli_locator.h"
#include "util/i18n.h"
#include "util/log_bridge.h"
#include "util/project_utils.h"
#include "util/settings.h"

namespace fs = std::filesystem;

namespace {

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

std::string ResolveWorkspaceDir() {
    fs::path base = fs::current_path();
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        base = fs::path(exe_path).parent_path();
    }
#endif
    fs::path workspace = base / "scripts";
    std::error_code ec;
    fs::create_directories(workspace, ec);
    return workspace.string();
}

std::string ResolvePluginsDir() {
    fs::path base = fs::current_path();
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        base = fs::path(exe_path).parent_path();
    }
#endif
    fs::path plugins_dir = base / "plugins";
    std::error_code ec;
    fs::create_directories(plugins_dir, ec);
    return plugins_dir.string();
}

constexpr float kTitleBarHeight = 34.0f;

constexpr float kTitleBarGap = 6.0f;

// Fase 7: width of the fixed vertical icon strip at the left edge of the
// workbench (DrawActivityBar) -- same footprint class as the title bar's
// own caption buttons (kButtonWidth = 46 in titlebar_panel.cpp).
constexpr float kActivityBarWidth = 44.0f;

bool g_native_close_requested = false;

void GlfwWindowCloseRequested(GLFWwindow* window) {
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    g_native_close_requested = true;
}

}

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1400, 900, "Ava Studio", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowCloseCallback(window, GlfwWindowCloseRequested);

    studio::titlebar::Install(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    studio::LoadDefaultFont(16.0f);

    studio::LoadBoldFont(16.0f);

    static std::string ini_path = [] {
        fs::path config_dir;
#if defined(_WIN32)
        if (const char* appdata = std::getenv("APPDATA")) {
            config_dir = fs::path(appdata) / "AvaStudio";
        } else {
            config_dir = "AvaStudio";
        }
#else
        if (const char* home = std::getenv("HOME")) {
            config_dir = fs::path(home) / ".config" / "AvaStudio";
        } else {
            config_dir = "AvaStudio";
        }
#endif
        std::error_code ec;
        fs::create_directories(config_dir, ec);
        return (config_dir / "imgui.ini").string();
    }();
    io.IniFilename = ini_path.c_str();

    studio::ApplyVSCodeDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    studio::EngineBridge engine;

    studio::StudioSettings settings = studio::LoadSettings();
    studio::util::SetLocale(studio::util::LocaleFromString(settings.language));
    engine.SetModulesPath(settings.modules_path);

    std::string pending_settings_browse;

    studio::BuildPanelState build_panel_state;
    studio::BuildBrowseField pending_build_browse_field = studio::BuildBrowseField::kNone;
    std::string pending_build_browse_value;

    studio::ExplorerState explorer_state;
    explorer_state.root_dir = ResolveWorkspaceDir();

    studio::EditorState editor_state;
    studio::InitEditorPanel(editor_state);

    editor_state.project_root = explorer_state.root_dir;
    studio::OpenWelcomeTab(editor_state);

    studio::TerminalState terminal_state;
    studio::LogsState logs_state;
    studio::ProblemsState problems_state;
    studio::FindInProjectState find_in_project_state;

    studio::LogBridge log_bridge;
    studio::PropertiesState properties_state;

    editor_state.log_bridge = &log_bridge;

    studio::EngineBridge::DemoTree demo_tree = engine.BuildDemoComponentTree();

    studio::PluginHostCallbacks plugin_callbacks;
    plugin_callbacks.get_project_root = [&]() -> std::string { return explorer_state.root_dir; };
    plugin_callbacks.get_active_file = [&](std::string& path, std::string& content) -> bool {
        const studio::EditorTab* active = editor_state.Active();
        if (!active || active->is_welcome) return false;
        path = active->file_path;
        content = active->GetText();
        return true;
    };

    plugin_callbacks.get_active_avaui_document = [&](std::string& path, std::string& avaui_source) -> bool {
        const studio::EditorTab* active = editor_state.Active();
        if (!active || active->is_welcome || !active->is_avaui) return false;
        path = active->file_path;
            avaui_source = (active->view_mode == studio::TabViewMode::Design)
                               ? [&] {
                                     avalang::ui::parser::AvauiWriteOptions opts;
                                     opts.code_behind = active->design.code_behind;
                                     opts.imports = active->design.imports;
                                     opts.initial_state.reserve(active->design.initial_state.size());
                                     for (const auto& row : active->design.initial_state) {
                                         opts.initial_state.push_back({row.key, row.value});
                                     }
                                     return avalang::ui::parser::WriteAvaui(active->design.Root(), opts);
                                 }()
                               : active->GetText();
        return true;
    };
    plugin_callbacks.get_last_run_output = [&](std::string& text, bool& had_error) -> bool {
        if (!terminal_state.has_run_result) return false;
        text = terminal_state.last_run.message;
        had_error = !terminal_state.last_run.success;
        return true;
    };
    plugin_callbacks.log = [&](const std::string& line) { log_bridge.Log(line); };

    auto perform_run = [&]() -> studio::RunProjectResult {
        studio::RunProjectResult result;
        const studio::EditorTab* active = editor_state.Active();
        if (!active || active->is_welcome) {
            result.has_result = false;
            result.error = "no hay ningun archivo abierto para ejecutar";
            return result;
        }

        studio::ClearErrorHighlights(editor_state);
        const std::string run_source_name = active->file_path;
        terminal_state.last_run = engine.RunScript(active->GetText(), run_source_name);
        terminal_state.has_run_result = true;
        studio::UpdateProblemsFromResult(problems_state, "run", terminal_state.last_run, run_source_name);
        if (!terminal_state.last_run.success) {

            const std::string& err_source = terminal_state.last_run.error_source;
            const std::string& target_path = err_source.empty() ? run_source_name : err_source;
            if (!target_path.empty() && target_path != run_source_name) {
                studio::OpenFileInTab(editor_state, target_path);
            }
            studio::HighlightError(editor_state, target_path, terminal_state.last_run.error_line,
                                    terminal_state.last_run.error_column, terminal_state.last_run.message);
        }

        result.has_result = true;
        result.had_error = !terminal_state.last_run.success;
        result.output = terminal_state.last_run.message;
        return result;
    };

    // Shared by "Run Project" and "Check" so both agree on exactly the same
    // project_dir/entry -- same settings fields, same DetectEntryFile() call,
    // same error strings, so they can never diverge on "which is the entry
    // point of this project".
    struct ProjectEntryResolution {
        bool ok = false;
        std::string error;
        fs::path project_dir;
        fs::path entry_path;
    };
    auto resolve_project_entry = [&]() -> ProjectEntryResolution {
        ProjectEntryResolution res;
        const fs::path project_dir = settings.build_project_dir.empty()
                                          ? fs::path(explorer_state.root_dir)
                                          : fs::path(settings.build_project_dir);
        std::error_code project_dir_ec;
        if (!fs::exists(project_dir, project_dir_ec) || !fs::is_directory(project_dir, project_dir_ec)) {
            res.error = "project folder not found -- check it under Build > Project.";
            return res;
        }
        const std::string entry = settings.build_entry_file.empty()
                                       ? studio::DetectEntryFile(project_dir)
                                       : settings.build_entry_file;
        if (entry.empty()) {
            res.error = "no .ava entry file found in the project -- set one under Build > Project.";
            return res;
        }
        res.ok = true;
        res.project_dir = project_dir;
        res.entry_path = project_dir / entry;
        return res;
    };

    plugin_callbacks.write_approved_edit = [&](const studio::PendingEdit& edit) {
        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(fs::path(explorer_state.root_dir) / edit.path, ec);
        if (ec) {
            log_bridge.Log("[plugin_host] no se pudo resolver la ruta al aplicar el cambio: " + edit.path);
            return;
        }

        fs::create_directories(resolved.parent_path(), ec);

        std::ofstream out(resolved, std::ios::binary | std::ios::trunc);
        if (!out) {
            log_bridge.Log("[plugin_host] no se pudo escribir el archivo: " + edit.path);
            return;
        }
        out << edit.new_content;
        out.close();

        for (const auto& tab_ptr : editor_state.tabs) {
            if (tab_ptr->file_path.empty()) continue;
            std::error_code ec2;
            fs::path tab_path = fs::weakly_canonical(tab_ptr->file_path, ec2);
            if (ec2 || tab_path != resolved) continue;

            tab_ptr->SetText(edit.new_content);
            tab_ptr->dirty = false;

            if (tab_ptr->is_avaui) {
                std::string parse_error;
                studio::design::DesignDocument parsed_doc;
                if (studio::design::LoadAvauiFile("", parsed_doc, parse_error)) {
                    tab_ptr->design = std::move(parsed_doc);
                    tab_ptr->design.dirty = false;
                    tab_ptr->avaui_load_error.clear();
                }
            }
            break;
        }

        log_bridge.Log("[plugin_host] cambio aplicado: " + edit.path);
    };
    plugin_callbacks.run_project_on_main_thread = perform_run;

    const std::string plugins_dir = ResolvePluginsDir();

    studio::PluginHost plugin_host(std::move(plugin_callbacks));
    plugin_host.LoadAll(plugins_dir, settings.disabled_plugins);

    std::unordered_map<std::string, bool> panel_open;
    for (const std::string& name : settings.closed_panels) {
        panel_open[name] = false;
    }

    auto persist_if_closed = [&](const std::string& name, bool open) {
        if (open) return;
        auto& closed = settings.closed_panels;
        if (std::find(closed.begin(), closed.end(), name) == closed.end()) {
            closed.push_back(name);
            studio::SaveSettings(settings);
        }
    };

    // Fase 7: reads settings.closed_panels directly instead of the
    // panel_open map -- panel_open only gets an entry for a given panel the
    // first time that panel's own try_emplace runs later in the same frame,
    // so at the point the Activity Bar needs to know "is Explorer open
    // right now" (drawn before Explorer itself), panel_open might not have
    // an entry for it yet on the very first frame. closed_panels is the
    // actual source of truth both panel_open and persist_if_closed already
    // derive from, so reading it directly sidesteps the ordering problem
    // instead of working around it.
    auto panel_visible = [&](const std::string& name) {
        auto& closed = settings.closed_panels;
        return std::find(closed.begin(), closed.end(), name) == closed.end();
    };

    // Flips `name`'s open/closed state (same toggle the View menu's
    // per-panel MenuItem list used to drive inline before Fase 7 moved that
    // list to the Activity Bar) -- pulled out to a lambda so the Command
    // Palette's per-panel "View: <panel>" entries (Fase 3) and the Activity
    // Bar's icon clicks (Fase 7) can both reuse it verbatim instead of
    // duplicating the find/erase/push_back dance.
    auto toggle_panel_visibility = [&](const std::string& name) {
        auto& closed = settings.closed_panels;
        auto it = std::find(closed.begin(), closed.end(), name);
        if (it != closed.end()) {
            closed.erase(it);
            panel_open[name] = true;
            ImGui::SetWindowFocus(name.c_str());
        } else {
            closed.push_back(name);
            panel_open[name] = false;
        }
        studio::SaveSettings(settings);
    };

    // Unconditionally opens+focuses `name` (same shape titlebar_result.open_settings_requested/
    // build_requested and want_build already had inline, duplicated three times) -- used for
    // panels that only ever need to be *shown*, never toggled closed, from a menu/shortcut/command
    // (Settings, Build).
    auto open_panel_focused = [&](const std::string& name) {
        auto& closed = settings.closed_panels;
        auto it = std::find(closed.begin(), closed.end(), name);
        if (it != closed.end()) closed.erase(it);
        panel_open[name] = true;
        ImGui::SetWindowFocus(name.c_str());
        studio::SaveSettings(settings);
    };

    studio::CommandPaletteState command_palette_state;
    studio::QuickOpenState quick_open_state;
    studio::NewProjectState new_project_state;

    // Fase 7: set by the Activity Bar's Extensions icon (activity_result.
    // extensions_clicked, below) and consumed the *next* frame when
    // DrawTitleBar is called -- the Activity Bar draws after the title bar
    // within the same frame, so a same-frame open isn't possible without
    // reordering the whole draw sequence; a one-frame-later modal open is
    // the same latency every other cross-panel effect in this loop already
    // has (e.g. Quick Open picks, Problems/Terminal file clicks).
    bool pending_open_extensions = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        plugin_host.PumpMainThreadWork();

        studio::PollScriptRun(terminal_state, engine, editor_state);
        studio::PollBuild(build_panel_state, log_bridge);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        const bool is_maximized = studio::titlebar::IsWindowMaximizedNow(window);

        const std::vector<studio::PluginInfo> available_plugins =
            plugin_host.ScanAvailable(plugins_dir, settings.disabled_plugins);
        studio::TitleBarResult titlebar_result =
            studio::DrawTitleBar(editor_state, settings, is_maximized, kTitleBarHeight, available_plugins,
                                  pending_open_extensions);
        pending_open_extensions = false;

        if (!titlebar_result.plugin_toggle_requested.empty()) {
            const std::string& name = titlebar_result.plugin_toggle_requested;
            auto& disabled = settings.disabled_plugins;
            auto it = std::find(disabled.begin(), disabled.end(), name);
            if (it != disabled.end()) {
                disabled.erase(it);
            } else {
                disabled.push_back(name);
            }
            studio::SaveSettings(settings);
            plugin_host.Reload(plugins_dir, settings.disabled_plugins);
        }

        if (titlebar_result.open_settings_requested || editor_state.open_settings_panel_requested) {
            open_panel_focused("Settings###settings");
        }

        if (titlebar_result.minimize_clicked) {
            glfwIconifyWindow(window);
        }
        if (titlebar_result.maximize_or_restore_clicked) {
            if (is_maximized) {
                glfwRestoreWindow(window);
            } else {
                glfwMaximizeWindow(window);
            }
        }

        bool want_quit = titlebar_result.close_clicked;

        auto to_rect = [](const studio::ScreenRect& r) {
            return studio::titlebar::Rect{static_cast<int>(r.min_x), static_cast<int>(r.min_y),
                                           static_cast<int>(r.max_x), static_cast<int>(r.max_y)};
        };
        studio::titlebar::Rect extra_rects[6] = {
            to_rect(titlebar_result.file_menu_rect),
            to_rect(titlebar_result.edit_menu_rect),
            to_rect(titlebar_result.view_menu_rect),
            to_rect(titlebar_result.run_menu_rect),
            to_rect(titlebar_result.about_rect),
        };
        int extra_rect_count = 5;
        if (titlebar_result.any_popup_open) {

            extra_rects[5] = studio::titlebar::Rect{
                static_cast<int>(viewport->WorkPos.x), static_cast<int>(viewport->WorkPos.y),
                static_cast<int>(viewport->WorkPos.x + viewport->WorkSize.x),
                static_cast<int>(viewport->WorkPos.y + kTitleBarHeight)};
            extra_rect_count = 6;
        }
        studio::titlebar::UpdateHitRegions(static_cast<int>(kTitleBarHeight), to_rect(titlebar_result.minimize_rect),
                                            to_rect(titlebar_result.maximize_rect), to_rect(titlebar_result.close_rect),
                                            extra_rects, extra_rect_count);

        // Fase 7: the Activity Bar occupies a fixed-width strip to the left
        // of the dockspace (same non-dockable-top-level-window idiom the
        // title bar itself uses), so the dockspace host is pushed right and
        // narrowed by that same width instead of starting flush against
        // the work area's left edge like it did before this phase.
        const float activity_bar_x = viewport->WorkPos.x;
        const float activity_bar_y = viewport->WorkPos.y + kTitleBarHeight + kTitleBarGap;
        const float activity_bar_h = viewport->WorkSize.y - kTitleBarHeight - kTitleBarGap;

        const ImVec2 dock_pos(activity_bar_x + kActivityBarWidth, activity_bar_y);
        const ImVec2 dock_size(viewport->WorkSize.x - kActivityBarWidth, activity_bar_h);
        ImGui::SetNextWindowPos(dock_pos);
        ImGui::SetNextWindowSize(dock_size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("AvaStudioDockHost", nullptr, host_flags);
        ImGui::PopStyleVar(3);

        // Bumped from "AvaStudioDockspace" so users upgrading from a build
        // that predates the "Build" panel (or any other default-layout fix)
        // get a fresh, fully-docked layout instead of inheriting a saved
        // imgui.ini where that panel has no dock slot and opens floating.
        ImGuiID dockspace_id = ImGui::GetID("AvaStudioDockspace_v2");

        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, dock_size);

            ImGuiID dock_main = dockspace_id;
            ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.28f, nullptr, &dock_main);
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, nullptr, &dock_main);
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.22f, nullptr, &dock_main);
            ImGuiID dock_center = dock_main;

            ImGui::DockBuilderDockWindow("Explorer###explorer", dock_left);

            ImGui::DockBuilderDockWindow("Toolbox###toolbox", dock_left);
            ImGui::DockBuilderDockWindow("Code Editor###code_editor", dock_center);
            ImGui::DockBuilderDockWindow("Properties###properties", dock_right);
            ImGui::DockBuilderDockWindow("Settings###settings", dock_right);
            ImGui::DockBuilderDockWindow("Preview###preview", dock_bottom);
            ImGui::DockBuilderDockWindow("Terminal###terminal", dock_bottom);
            ImGui::DockBuilderDockWindow("Build###build", dock_bottom);
            ImGui::DockBuilderDockWindow("Logs###logs", dock_bottom);
            ImGui::DockBuilderDockWindow("Problems###problems", dock_bottom);
            ImGui::DockBuilderDockWindow("Find in Project###find_in_project", dock_bottom);

            for (const auto& panel : plugin_host.Panels()) {
                ImGuiID target = dock_center;
                switch (panel.default_dock_slot) {
                    case AVA_DOCK_LEFT: target = dock_left; break;
                    case AVA_DOCK_RIGHT: target = dock_right; break;
                    case AVA_DOCK_BOTTOM: target = dock_bottom; break;
                    case AVA_DOCK_CENTER: target = dock_center; break;
                }
                ImGui::DockBuilderDockWindow(panel.name.c_str(), target);
            }

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
        ImGui::End();

        // Fase 7: Activity Bar drawn as its own fixed strip immediately
        // left of the dockspace host, same relationship the title bar has
        // to everything below it. Explorer/Toolbox/Settings are toggled
        // the same way the old View menu list did (open/close); Search
        // reuses the exact editor_state.find_in_project_requested flag the
        // Ctrl+Shift+F handler and Command Palette entry already set
        // further down, instead of re-deriving its own open/focus/
        // focus_query_field dance; Extensions defers to pending_open_extensions
        // (see its declaration above) since the modal itself lives inside
        // DrawTitleBar, called earlier this same frame.
        const studio::ActivityBarResult activity_result = studio::DrawActivityBar(
            activity_bar_x, activity_bar_y, kActivityBarWidth, activity_bar_h,
            panel_visible("Explorer###explorer"), panel_visible("Find in Project###find_in_project"),
            panel_visible("Toolbox###toolbox"), panel_visible("Settings###settings"));

        if (activity_result.explorer_clicked) {
            toggle_panel_visibility("Explorer###explorer");
        }
        if (activity_result.toolbox_clicked) {
            toggle_panel_visibility("Toolbox###toolbox");
        }
        if (activity_result.settings_clicked) {
            toggle_panel_visibility("Settings###settings");
        }
        if (activity_result.search_clicked) {
            editor_state.find_in_project_requested = true;
        }
        if (activity_result.extensions_clicked) {
            pending_open_extensions = true;
        }

        studio::ExplorerResult explorer_result;
        if (bool& open = panel_open.try_emplace("Explorer###explorer", true).first->second; open) {
            explorer_result = studio::DrawExplorerPanel(explorer_state, &open);
            persist_if_closed("Explorer###explorer", open);
        }
        if (explorer_result.file_to_open) {
            studio::OpenFileInTab(editor_state, *explorer_result.file_to_open);
        }
        if (explorer_result.file_deleted) {
            studio::CloseTabForPath(editor_state, *explorer_result.file_deleted);
        }
        if (explorer_result.file_renamed) {
            studio::RenameTabPath(editor_state, explorer_result.file_renamed->first,
                                   explorer_result.file_renamed->second);
        }
        if (explorer_result.reveal_in_file_manager) {
            studio::titlebar::RevealInFileExplorer(*explorer_result.reveal_in_file_manager);
        }

        // Fase 7: Toolbox becomes a real panel_open entry (same shape as
        // every other panel below) instead of always drawing without a
        // close button whenever the active tab happens to be an .avaui
        // Design view -- needed so the new Activity Bar (and Command
        // Palette's "View: Toolbox" entry, which already iterates
        // kBuiltinPanelNames) has something to toggle. The relevant-context
        // gate stays: it still only actually draws while the active tab is
        // an .avaui in Design view, same as before.
        if (bool& open = panel_open.try_emplace("Toolbox###toolbox", true).first->second; open) {
            if (const studio::EditorTab* active = editor_state.Active();
                active && active->is_avaui && active->view_mode == studio::TabViewMode::Design) {
                studio::DrawToolboxPanel(&open);
                persist_if_closed("Toolbox###toolbox", open);
            }
        }

        studio::DrawEditorPanel(editor_state);
        if (const studio::EditorTab* active = editor_state.Active();
            active && active->is_avaui && active->view_mode == studio::TabViewMode::Design) {

            properties_state = editor_state.designer_selection ? *editor_state.designer_selection
                                                                 : studio::PropertiesState{};
        }

        ImGuiIO& io = ImGui::GetIO();
        const bool want_save    = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S);
        const bool want_save_as = io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S);
        const bool want_new     = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N);
        const bool want_open    = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O);
        const bool want_close_tab = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W);
        const bool want_run     = !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5);
        const bool want_run_project = io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5);
        const bool want_build   = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_B);
        const bool want_check   = io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_B);
        const bool want_toggle_view = ImGui::IsKeyPressed(ImGuiKey_F7);
        // Gated on !code_editor_has_focus: the ImGuiColorTextEdit widget
        // already binds Ctrl+Shift+F to "select all occurrences in this
        // file" internally (only while its own child window has focus), so
        // without this guard both would fire together -- same collision
        // category as want_run/Shift+F5 (Fase 1) and want_build/Ctrl+Shift+B
        // (Fase 2), gated on focus here instead of a modifier key since the
        // conflicting binding lives inside a third-party widget we don't
        // own. When the editor does have focus, Ctrl+Shift+F still reaches
        // the widget's own "select all occurrences" -- the Edit menu item
        // and this global shortcut both still work whenever it doesn't.
        const bool want_find_in_project =
            !editor_state.code_editor_has_focus && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F);
        // No focus-gating needed here the way Find in Project's Ctrl+Shift+F
        // needed one against ImGuiColorTextEdit -- the palettes/patches
        // vendored under avastudio/patches don't touch 'P', and nothing else
        // in this codebase binds Ctrl+Shift+P today.
        const bool want_command_palette = io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P);
        // Same no-focus-gating reasoning as want_command_palette above --
        // nothing vendored under avastudio/patches binds plain Ctrl+P, and
        // no other shortcut in this codebase uses it. !io.KeyShift keeps
        // this from also firing alongside Ctrl+Shift+P.
        const bool want_quick_open = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P);

        if (titlebar_result.new_requested || want_new || editor_state.new_tab_requested) {
            studio::NewUntitledTab(editor_state);
        }
        if (titlebar_result.open_requested || want_open || editor_state.open_requested) {
            std::string path;
            if (studio::titlebar::OpenFileDialog(window, path, explorer_state.root_dir)) {
                studio::OpenFileInTab(editor_state, path);
            }
        }
        if (titlebar_result.open_folder_requested || editor_state.open_folder_requested) {
            std::string path;
            if (studio::titlebar::OpenFolderDialog(window, path, explorer_state.root_dir)) {
                // Bug found while wiring Fase 6 (New Project): this only
                // ever updated explorer_state.root_dir -- editor_state.
                // project_root (what Quick Open/Find in Project actually
                // search) was left pointing at whatever project was open
                // before, so switching folders here silently left those
                // two features searching the old project. Same fix shape
                // as want_run/Shift+F5 (Fase 1) and want_build/Ctrl+Shift+B
                // (Fase 2): keep the two in sync at the one place that
                // changes either of them.
                explorer_state.root_dir = path;
                editor_state.project_root = path;
            }
        }
        if (titlebar_result.save_as_requested || want_save_as || editor_state.save_as_requested) {
            if (studio::EditorTab* active = editor_state.Active(); active && !active->is_welcome) {
                std::string path;
                if (studio::titlebar::SaveFileDialog(window, path, explorer_state.root_dir)) {
                    active->file_path = path;
                    studio::SaveActiveTab(editor_state);
                }
            }
        }
        if (editor_state.save_requested || want_save) {
            if (studio::EditorTab* active = editor_state.Active(); active && !active->is_welcome) {

                if (active->file_path.empty()) {
                    std::string path;
                    if (studio::titlebar::SaveFileDialog(window, path, explorer_state.root_dir)) {
                        active->file_path = path;
                        studio::SaveActiveTab(editor_state);
                    }
                } else {
                    studio::SaveActiveTab(editor_state);
                }
            }
        }
        if (editor_state.close_tab_requested || want_close_tab) {
            if (editor_state.active_tab >= 0) {
                studio::RequestCloseTab(editor_state, editor_state.active_tab);
            }
        }
        if (want_toggle_view) {

            if (studio::EditorTab* active = editor_state.Active()) {
                studio::ToggleTabViewMode(*active);
            }
        }
        if (editor_state.run_requested || want_run) {

            if (studio::EditorTab* active = editor_state.Active(); active && !active->is_welcome) {
                if (terminal_state.run.running.load()) {

                } else if (active->file_path.empty()) {
                    engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error,
                        "save the file (Ctrl+S) before running it -- Run needs a .ava on disk.");
                } else {
                    studio::SaveTab(*active);
                    fs::path ava_cli = settings.build_ava_cli_path.empty() ? studio::DetectAvaCliPath()
                                                                            : fs::path(settings.build_ava_cli_path);
                    std::error_code ava_cli_ec;
                    if (ava_cli.empty() || !fs::exists(ava_cli, ava_cli_ec)) {
                        engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error,
                            "could not find ava_cli(.exe) -- set its path under Build > Advanced.");
                    } else {
                        studio::StartScriptRun(terminal_state, engine, ava_cli.string(), active->file_path);
                    }
                }
            }
        }

        if (editor_state.run_project_requested || want_run_project) {

            if (terminal_state.run.running.load()) {

            } else {
                const ProjectEntryResolution resolved = resolve_project_entry();
                if (!resolved.ok) {
                    engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error, resolved.error);
                } else {
                    fs::path ava_cli = settings.build_ava_cli_path.empty() ? studio::DetectAvaCliPath()
                                                                            : fs::path(settings.build_ava_cli_path);
                    std::error_code ava_cli_ec;
                    if (ava_cli.empty() || !fs::exists(ava_cli, ava_cli_ec)) {
                        engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error,
                            "could not find ava_cli(.exe) -- set its path under Build > Advanced.");
                    } else {
                        studio::SaveAllTabs(editor_state);
                        studio::StartScriptRun(terminal_state, engine, ava_cli.string(), resolved.entry_path.string());
                    }
                }
            }
        }

        if (editor_state.check_requested || want_check) {

            const ProjectEntryResolution resolved = resolve_project_entry();
            if (!resolved.ok) {
                engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error, resolved.error);
            } else {
                studio::SaveAllTabs(editor_state);

                std::ifstream entry_file(resolved.entry_path, std::ios::binary);
                if (!entry_file) {
                    engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error,
                        "could not read entry file -- " + resolved.entry_path.string());
                } else {
                    std::ostringstream entry_source_stream;
                    entry_source_stream << entry_file.rdbuf();
                    const std::string entry_path_str = resolved.entry_path.string();

                    const studio::RunResult check_result = engine.CheckScript(entry_source_stream.str(), entry_path_str);
                    studio::UpdateProblemsFromResult(problems_state, "check", check_result, entry_path_str);

                    if (check_result.success) {
                        studio::ClearErrorHighlights(editor_state);
                    } else {
                        const std::string& target_path =
                            check_result.error_source.empty() ? entry_path_str : check_result.error_source;
                        if (const studio::EditorTab* active = editor_state.Active();
                            target_path.empty() || !active || active->file_path != target_path) {
                            studio::ClearErrorHighlights(editor_state);
                            if (!target_path.empty()) studio::OpenFileInTab(editor_state, target_path);
                        } else {
                            studio::ClearErrorHighlights(editor_state);
                        }
                        studio::HighlightError(editor_state, target_path, check_result.error_line,
                                                check_result.error_column, check_result.message);
                    }
                }
            }
        }

        if (titlebar_result.build_requested || editor_state.build_requested || want_build) {
            // Build panel only configures paths now (Target/Project/Advanced) -- this is the
            // one place a build actually starts, same shape as run_project_requested/
            // check_requested just above: resolve_project_entry's project_dir default (the
            // folder currently open in the editor) is exactly what TriggerBuild falls back to
            // via ResolveBuildProjectDir when Build's own Project Folder override is empty.
            const studio::TriggerBuildOutcome build_outcome =
                studio::TriggerBuild(build_panel_state, settings, explorer_state.root_dir, log_bridge);

            // The configured/detected entry .ava doesn't exist on disk (typically a stale
            // build_entry_file left over from a different project folder, see the "es cierto
            // el archivo no estaba" thread above). Rather than making the person go dig it out
            // by hand in the Build panel, offer the same picker Browse uses right here, then
            // retry the build immediately with whatever they picked.
            if (build_outcome.entry_file_missing) {
                std::string path;
                if (studio::titlebar::OpenFileDialog(window, path, build_outcome.project_dir)) {
                    settings.build_entry_file = studio::NormalizeEntryFilePath(build_outcome.project_dir, path);
                    studio::SaveSettings(settings);
                    studio::TriggerBuild(build_panel_state, settings, explorer_state.root_dir, log_bridge);
                }
            }
        }

        if (editor_state.find_in_project_requested || want_find_in_project) {
            auto& closed = settings.closed_panels;
            auto it = std::find(closed.begin(), closed.end(), "Find in Project###find_in_project");
            if (it != closed.end()) closed.erase(it);
            panel_open["Find in Project###find_in_project"] = true;
            ImGui::SetWindowFocus("Find in Project###find_in_project");
            find_in_project_state.focus_query_field = true;
            studio::SaveSettings(settings);
        }

        if (want_command_palette || titlebar_result.command_palette_requested) {
            studio::OpenCommandPalette(command_palette_state);
        }

        if (want_quick_open || titlebar_result.quick_open_requested || editor_state.quick_open_requested) {
            studio::OpenQuickOpen(quick_open_state, editor_state.project_root);
        }

        if (titlebar_result.new_project_requested || editor_state.new_project_requested) {
            studio::OpenNewProjectDialog(new_project_state, explorer_state.root_dir);
        }

        want_quit = want_quit || titlebar_result.quit_requested || g_native_close_requested;
        g_native_close_requested = false;

        if (want_quit) {
            if (studio::HasUnsavedChanges(editor_state)) {
                ImGui::OpenPopup("Unsaved Changes##ExitConfirm");
            } else {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f));
        if (ImGui::BeginPopupModal("Unsaved Changes##ExitConfirm", nullptr,
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextWrapped("You have unsaved files. Do you want to save your changes before exiting?");
            ImGui::Spacing();
            ImGui::TextDisabled("Unsaved changes will be lost if you don't save them.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float button_w = (ImGui::GetContentRegionAvail().x - 2.0f * spacing) / 3.0f;

            ImGui::PushStyleColor(ImGuiCol_Button, studio::palette::FromHex(studio::palette::kSuccess, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, studio::palette::FromHex(studio::palette::kSuccess, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, studio::palette::FromHex(0x189b48));
            if (ImGui::Button("Save All & Exit", ImVec2(button_w, 0.0f))) {
                studio::SaveAllTabs(editor_state);

                bool all_saved = true;
                for (auto& tab : editor_state.tabs) {
                    if (tab->is_welcome || !tab->dirty || !tab->file_path.empty()) continue;
                    std::string path;
                    if (studio::titlebar::SaveFileDialog(window, path, explorer_state.root_dir)) {
                        tab->file_path = path;
                        studio::SaveTab(*tab);
                    } else {
                        all_saved = false;
                    }
                }
                if (all_saved) glfwSetWindowShouldClose(window, GLFW_TRUE);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0.0f, spacing);

            ImGui::PushStyleColor(ImGuiCol_Button, studio::palette::FromHex(studio::palette::kError, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, studio::palette::FromHex(studio::palette::kError, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, studio::palette::FromHex(0xc93b3b));

            if (ImGui::Button("Exit Without Saving", ImVec2(button_w, 0.0f))) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0.0f, spacing);

            if (ImGui::Button("Cancel", ImVec2(button_w, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        editor_state.save_requested = false;
        editor_state.run_requested = false;
        editor_state.run_project_requested = false;
        editor_state.check_requested = false;
        editor_state.find_in_project_requested = false;
        editor_state.close_tab_requested = false;
        editor_state.open_requested = false;
        editor_state.open_folder_requested = false;
        editor_state.new_tab_requested = false;
        editor_state.save_as_requested = false;
        editor_state.open_settings_panel_requested = false;
        editor_state.build_requested = false;
        editor_state.quick_open_requested = false;
        editor_state.new_project_requested = false;

        if (bool& open = panel_open.try_emplace("Preview###preview", true).first->second; open) {
            if (auto selected = studio::DrawPreviewPanel(demo_tree.root, &open)) {
                properties_state = *selected;
            }
            persist_if_closed("Preview###preview", open);
        }

        if (bool& open = panel_open.try_emplace("Properties###properties", true).first->second; open) {
            if (auto edit = studio::DrawPropertiesPanel(properties_state, &open)) {
                for (auto& tab_ptr : editor_state.tabs) {
                studio::EditorTab& tab = *tab_ptr;
                if (tab.id != edit->tab_id || !tab.is_avaui) continue;
                if (avalang::ui::IComponent* node =
                        studio::design::FindNodeById(tab.design.Root(), edit->node_id)) {
                    switch (edit->kind) {
                        case studio::PropertyEditKind::kValue:
                            node->SetProperty(edit->key, avalang::ui::PropertyValue(edit->new_value));
                            break;
                        case studio::PropertyEditKind::kId:
                            node->SetProperty("id", avalang::ui::PropertyValue(edit->new_value));
                            break;
                        case studio::PropertyEditKind::kType:
                            break;
                        case studio::PropertyEditKind::kAddProperty:
                            if (!node->HasProperty(edit->key)) {
                                node->SetProperty(edit->key, avalang::ui::PropertyValue(edit->new_value));
                            }
                            break;
                        case studio::PropertyEditKind::kRemoveProperty:
                            node->RemoveProperty(edit->key);
                            break;
                        case studio::PropertyEditKind::kEvent:
                            node->SetProperty(edit->key, avalang::ui::PropertyValue(edit->new_value));
                            break;
                        case studio::PropertyEditKind::kRemoveEvent:
                            node->RemoveProperty(edit->key);
                            break;
                    }
                    tab.design.dirty = true;
                    tab.dirty = true;
                }

                break;
            }
            }
            persist_if_closed("Properties###properties", open);
        }

        if (bool& open = panel_open.try_emplace("Settings###settings", true).first->second; open) {
            bool settings_dirty = false;
            bool settings_browse_requested = false;
            studio::DrawSettingsPanel(settings, plugin_host.SettingsPanels(), settings_dirty,
                                       settings_browse_requested, pending_settings_browse, &open);
            pending_settings_browse.clear();
            if (settings_browse_requested) {
                std::string path;
                if (studio::titlebar::OpenFolderDialog(window, path, settings.modules_path)) {
                    pending_settings_browse = path;
                }
            }
            if (settings_dirty) {
                engine.SetModulesPath(settings.modules_path);
                studio::SaveSettings(settings);
            }
            persist_if_closed("Settings###settings", open);
        }

        if (bool& open = panel_open.try_emplace("Build###build", true).first->second; open) {
            studio::BuildPanelResult build_result =
                studio::DrawBuildPanel(build_panel_state, settings, explorer_state.root_dir,
                                        pending_build_browse_field, pending_build_browse_value, log_bridge, &open);
            pending_build_browse_field = studio::BuildBrowseField::kNone;
            pending_build_browse_value.clear();
            if (build_result.browse_requested != studio::BuildBrowseField::kNone) {
                std::string path;
                bool picked = false;
                switch (build_result.browse_requested) {
                    case studio::BuildBrowseField::kProjectDir:
                        picked = studio::titlebar::OpenFolderDialog(
                            window, path,
                            settings.build_project_dir.empty() ? explorer_state.root_dir : settings.build_project_dir);
                        break;
                    case studio::BuildBrowseField::kOutputDir:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_out_dir);
                        break;
                    case studio::BuildBrowseField::kVcpkgRoot:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_vcpkg_root);
                        break;
                    case studio::BuildBrowseField::kCompilerPathDesktop:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_compiler_path_desktop);
                        break;
                    case studio::BuildBrowseField::kCompilerPathBarekernel:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_compiler_path_barekernel);
                        break;
                    case studio::BuildBrowseField::kEntryFile:
                        picked = studio::titlebar::OpenFileDialog(
                            window, path,
                            settings.build_project_dir.empty() ? explorer_state.root_dir : settings.build_project_dir);
                        break;
                    case studio::BuildBrowseField::kAvaCliPath:
                        // ava_cli(.exe) is an executable, not a .ava script -- the default
                        // OpenFileDialog filter (AvaLang Scripts) hid it from this picker
                        // entirely, which is what looked like "opens to search for a .ava
                        // instead of the file I actually need".
                        picked = studio::titlebar::OpenFileDialog(
                            window, path, settings.build_ava_cli_path,
                            "Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0");
                        break;
                    case studio::BuildBrowseField::kKeyFile:
                        // The AES key is 32 raw bytes with no fixed extension -- same
                        // "hidden behind the .ava filter" problem as ava_cli path above.
                        picked = studio::titlebar::OpenFileDialog(window, path, explorer_state.root_dir,
                                                                   "All Files (*.*)\0*.*\0");
                        break;
                    case studio::BuildBrowseField::kNone:
                        break;
                }
                if (picked) {
                    pending_build_browse_field = build_result.browse_requested;
                    pending_build_browse_value = path;
                }
            }
            if (build_result.settings_dirty) {
                studio::SaveSettings(settings);
            }
            persist_if_closed("Build###build", open);
        }

        if (bool& open = panel_open.try_emplace("Terminal###terminal", true).first->second; open) {
            if (auto file_click = studio::DrawTerminalPanel(terminal_state, engine, &open)) {

                if (const studio::EditorTab* active = editor_state.Active();
                    file_click->file_path.empty() || !active || active->file_path != file_click->file_path) {
                    studio::ClearErrorHighlights(editor_state);
                    if (!file_click->file_path.empty()) {
                        studio::OpenFileInTab(editor_state, file_click->file_path);
                    }
                } else {
                    studio::ClearErrorHighlights(editor_state);
                }
                studio::HighlightError(editor_state, file_click->file_path, file_click->line,
                                        file_click->column, file_click->message);
            }
            persist_if_closed("Terminal###terminal", open);
        }

        if (bool& open = panel_open.try_emplace("Logs###logs", true).first->second; open) {
            studio::DrawLogsPanel(logs_state, log_bridge, &open);
            persist_if_closed("Logs###logs", open);
        }

        if (bool& open = panel_open.try_emplace("Problems###problems", true).first->second; open) {
            if (auto file_click = studio::DrawProblemsPanel(problems_state, &open)) {

                if (const studio::EditorTab* active = editor_state.Active();
                    file_click->file_path.empty() || !active || active->file_path != file_click->file_path) {
                    studio::ClearErrorHighlights(editor_state);
                    if (!file_click->file_path.empty()) {
                        studio::OpenFileInTab(editor_state, file_click->file_path);
                    }
                } else {
                    studio::ClearErrorHighlights(editor_state);
                }
                studio::HighlightError(editor_state, file_click->file_path, file_click->line,
                                        file_click->column, file_click->message);
            }
            persist_if_closed("Problems###problems", open);
        }

        if (bool& open = panel_open.try_emplace("Find in Project###find_in_project", true).first->second; open) {
            if (auto file_click =
                    studio::DrawFindInProjectPanel(find_in_project_state, editor_state.project_root, &open)) {
                studio::OpenFileInTab(editor_state, file_click->file_path);
                studio::SelectMatchInEditor(editor_state, file_click->file_path, file_click->line,
                                             file_click->column_start, file_click->column_end);
            }
            persist_if_closed("Find in Project###find_in_project", open);
        }

        // Fase 3 (Command Palette): registry rebuilt every frame, by reference into
        // all the state above -- same "no separate source of truth" reasoning
        // DrawTitleBar's menus already follow (this is the same set of actions,
        // just also reachable through Ctrl+Shift+P / a searchable list instead of
        // only through the menu bar). Deliberately NOT included here: "About"
        // (a static-bool modal local to titlebar_panel.cpp with no external
        // trigger) and "Replace in Project"/whole-word/regex search (out of
        // §5.2's scope, see Fase 2.5's own notes). "Extensions" WAS excluded
        // for the same reason as "About" through Fase 6, but Fase 7 gave it a
        // real trigger (pending_open_extensions, for the Activity Bar's icon),
        // so it's included below now.
        {
            const std::string category_file = studio::util::Tr("menu.file");
            const std::string category_edit = studio::util::Tr("menu.edit");
            const std::string category_run = studio::util::Tr("menu.run");
            const std::string category_view = studio::util::Tr("menu.view");
            const std::string category_preferences = studio::util::Tr("menu.file.preferences");
            const std::string category_build = studio::util::Tr("panel.build.title");

            std::vector<studio::Command> commands;
            commands.reserve(24);

            auto add = [&](const std::string& category, const std::string& label_key, const std::string& shortcut,
                            std::function<void()> action) {
                commands.push_back(studio::Command{label_key, category + ": " + studio::util::Tr(label_key), shortcut,
                                                    std::move(action)});
            };

            add(category_file, "menu.file.new_project", "", [&] { editor_state.new_project_requested = true; });
            add(category_file, "menu.file.new_file", "Ctrl+N", [&] { editor_state.new_tab_requested = true; });
            add(category_file, "menu.file.open", "Ctrl+O", [&] { editor_state.open_requested = true; });
            add(category_file, "menu.file.open_folder", "", [&] { editor_state.open_folder_requested = true; });
            add(category_file, "menu.file.save", "Ctrl+S", [&] { editor_state.save_requested = true; });
            add(category_file, "menu.file.save_as", "Ctrl+Shift+S", [&] { editor_state.save_as_requested = true; });
            add(category_file, "menu.file.close_tab", "Ctrl+W", [&] { editor_state.close_tab_requested = true; });
            add(category_file, "menu.file.exit", "Alt+F4", [&] { g_native_close_requested = true; });

            add(category_edit, "menu.edit.quick_open", "Ctrl+P", [&] { editor_state.quick_open_requested = true; });
            add(category_edit, "menu.edit.find_in_project", "Ctrl+Shift+F",
                [&] { editor_state.find_in_project_requested = true; });

            add(category_run, "menu.run.run_script", "F5", [&] { editor_state.run_requested = true; });
            add(category_run, "menu.run.run_project", "Shift+F5", [&] { editor_state.run_project_requested = true; });
            add(category_run, "menu.run.check", "Ctrl+Shift+B", [&] { editor_state.check_requested = true; });
            add(category_run, "menu.run.build", "Ctrl+B", [&] { editor_state.build_requested = true; });

            add(category_preferences, "menu.file.settings", "Ctrl+,",
                [&] { editor_state.open_settings_panel_requested = true; });
            // Fase 7: now that the Activity Bar's Extensions icon gives the
            // Plugins modal a real external trigger (pending_open_extensions),
            // it's no longer a "static-bool modal with no entry point" the
            // way Fase 3's design note excluded it -- so it gets a Command
            // Palette entry the same way Settings just above does.
            add(category_preferences, "menu.file.extensions", "", [&] { pending_open_extensions = true; });

            {
                const studio::EditorTab* active = editor_state.Active();
                const bool showing_design =
                    active && active->is_avaui && active->view_mode == studio::TabViewMode::Design;
                add(category_view, showing_design ? "menu.file.view_code" : "menu.file.view_design", "F7", [&] {
                    if (studio::EditorTab* mutable_active = editor_state.Active()) {
                        studio::ToggleTabViewMode(*mutable_active);
                    }
                });
            }

            for (const studio::BuiltinPanelInfo& info : studio::kBuiltinPanelNames) {
                const std::string panel_id(info.id);
                const std::string panel_label =
                    info.tr_key.empty() ? std::string(info.fallback_label) : studio::util::Tr(std::string(info.tr_key));
                commands.push_back(studio::Command{"view.toggle." + panel_id, category_view + ": " + panel_label, "",
                                                    [&toggle_panel_visibility, panel_id] {
                                                        toggle_panel_visibility(panel_id);
                                                    }});
            }

            add(category_build, "build.install_vcpkg_button", "", [&] {
                studio::StartVcpkgInstall(build_panel_state, studio::ResolveVcpkgInstallTarget(settings),
                                           "x64-windows-static-md");
            });

            studio::DrawCommandPalette(command_palette_state, commands);
        }

        // Fase 4 (Quick Open): drawn unconditionally each frame, same
        // requirement as DrawCommandPalette just above -- it's a no-op
        // whenever the popup isn't open. A pick just opens the file, same
        // two-step (open then let the caller act) pattern already used for
        // Terminal/Problems/Find in Project file clicks.
        if (auto quick_open_pick = studio::DrawQuickOpen(quick_open_state)) {
            studio::OpenFileInTab(editor_state, *quick_open_pick);
        }

        // Fase 6 (New Project wizard): drawn unconditionally each frame,
        // same requirement as DrawCommandPalette/DrawQuickOpen above --
        // it's a no-op whenever the popup isn't open. Two things it can
        // hand back on a given frame: (a) a request to browse for the
        // destination folder -- handled the same way BuildBrowseField's
        // switch does in the Build panel block above, just with a single
        // bool instead of an enum (this dialog only ever browses one
        // field), and (b) a successfully created project -- same two-step
        // pattern as every other cross-panel effect in this app (Quick
        // Open picks, Problems/Terminal file clicks): point Explorer and
        // the rest of EditorState at the new project, then open its entry
        // file, instead of the dialog reaching into either of those
        // itself.
        {
            const studio::NewProjectDrawResult new_project_draw = studio::DrawNewProjectDialog(new_project_state);
            if (new_project_draw.browse_destination_requested) {
                std::string path;
                if (studio::titlebar::OpenFolderDialog(window, path, new_project_state.destination)) {
                    new_project_state.destination = path;
                }
            }
            if (new_project_draw.created) {
                explorer_state.root_dir = new_project_draw.created->project_dir;
                editor_state.project_root = new_project_draw.created->project_dir;
                studio::OpenFileInTab(editor_state, new_project_draw.created->entry_file);
            }
        }

        for (const auto& panel : plugin_host.Panels()) {
            bool& open = panel_open.try_emplace(panel.name, true).first->second;
            if (!open) continue;

            ImGui::Begin(panel.name.c_str(), &open);
            AvaPanelContext* ctx = studio::plugins_ui::BeginPanelContext(panel.name.c_str());
            panel.draw(ctx, panel.user_data);
            studio::plugins_ui::EndPanelContext(ctx);
            ImGui::End();

            persist_if_closed(panel.name, open);
        }

        studio::DrawPendingEditsPanel(plugin_host);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.043f, 0.043f, 0.051f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    plugin_host.UnloadAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

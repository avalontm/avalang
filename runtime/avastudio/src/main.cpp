







#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
#include "panels/builtin_panels.h"
#include "panels/editor_panel.h"
#include "panels/explorer_panel.h"
#include "panels/logs_panel.h"
#include "panels/pending_edits_panel.h"
#include "panels/preview_panel.h"
#include "panels/properties_panel.h"
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
#include "util/log_bridge.h"
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();







        plugin_host.PumpMainThreadWork();

        // Folds the background "Run" (StartScriptRun, see terminal_panel.h)
        // into the console/terminal_state.last_run as it streams in and
        // once it finishes. Unconditional -- must run even while the
        // Terminal panel tab is closed, see PollScriptRun's own comment.
        studio::PollScriptRun(terminal_state, engine);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();


        const bool is_maximized = studio::titlebar::IsWindowMaximizedNow(window);




        const std::vector<studio::PluginInfo> available_plugins =
            plugin_host.ScanAvailable(plugins_dir, settings.disabled_plugins);
        studio::TitleBarResult titlebar_result = studio::DrawTitleBar(
            editor_state, settings, is_maximized, kTitleBarHeight, available_plugins,
            plugin_host.Panels(), settings.closed_panels);





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








        if (!titlebar_result.panel_toggle_requested.empty()) {
            const std::string& name = titlebar_result.panel_toggle_requested;
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
        }

        if (titlebar_result.open_settings_requested) {
            auto& closed = settings.closed_panels;
            auto it = std::find(closed.begin(), closed.end(), "Settings");
            if (it != closed.end()) closed.erase(it);
            panel_open["Settings"] = true;
            ImGui::SetWindowFocus("Settings");
            studio::SaveSettings(settings);
        }

        if (titlebar_result.build_requested) {
            auto& closed = settings.closed_panels;
            auto it = std::find(closed.begin(), closed.end(), "Build");
            if (it != closed.end()) closed.erase(it);
            panel_open["Build"] = true;
            ImGui::SetWindowFocus("Build");
            studio::SaveSettings(settings);
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
        studio::titlebar::Rect extra_rects[5] = {
            to_rect(titlebar_result.file_menu_rect),
            to_rect(titlebar_result.view_menu_rect),
            to_rect(titlebar_result.run_menu_rect),
            to_rect(titlebar_result.about_rect),
        };
        int extra_rect_count = 4;
        if (titlebar_result.any_popup_open) {











            extra_rects[4] = studio::titlebar::Rect{
                static_cast<int>(viewport->WorkPos.x), static_cast<int>(viewport->WorkPos.y),
                static_cast<int>(viewport->WorkPos.x + viewport->WorkSize.x),
                static_cast<int>(viewport->WorkPos.y + kTitleBarHeight)};
            extra_rect_count = 5;
        }
        studio::titlebar::UpdateHitRegions(static_cast<int>(kTitleBarHeight), to_rect(titlebar_result.minimize_rect),
                                            to_rect(titlebar_result.maximize_rect), to_rect(titlebar_result.close_rect),
                                            extra_rects, extra_rect_count);


        const ImVec2 dock_pos(viewport->WorkPos.x, viewport->WorkPos.y + kTitleBarHeight + kTitleBarGap);
        const ImVec2 dock_size(viewport->WorkSize.x, viewport->WorkSize.y - kTitleBarHeight - kTitleBarGap);
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

        ImGuiID dockspace_id = ImGui::GetID("AvaStudioDockspace");






        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, dock_size);

            ImGuiID dock_main = dockspace_id;
            ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.28f, nullptr, &dock_main);
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, nullptr, &dock_main);
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.22f, nullptr, &dock_main);
            ImGuiID dock_center = dock_main;

            ImGui::DockBuilderDockWindow("Explorer", dock_left);







            ImGui::DockBuilderDockWindow("Toolbox", dock_left);
            ImGui::DockBuilderDockWindow("Code Editor", dock_center);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Settings", dock_right);
            ImGui::DockBuilderDockWindow("Preview", dock_bottom);
            ImGui::DockBuilderDockWindow("Terminal", dock_bottom);
            ImGui::DockBuilderDockWindow("Output", dock_bottom);






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









        studio::ExplorerResult explorer_result;
        if (bool& open = panel_open.try_emplace("Explorer", true).first->second; open) {
            explorer_result = studio::DrawExplorerPanel(explorer_state, &open);
            persist_if_closed("Explorer", open);
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








        if (const studio::EditorTab* active = editor_state.Active();
            active && active->is_avaui && active->view_mode == studio::TabViewMode::Design) {
            studio::DrawToolboxPanel();
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
        const bool want_run     = ImGui::IsKeyPressed(ImGuiKey_F5);
        const bool want_build   = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B);
        const bool want_toggle_view = ImGui::IsKeyPressed(ImGuiKey_F7);

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





                explorer_state.root_dir = path;
            }
        }
        if (titlebar_result.save_as_requested || want_save_as) {
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
            // Interactive Run (button/shortcut): out-of-process via
            // ava_cli.exe, see StartScriptRun's comment in
            // terminal_panel.h for why. This intentionally does NOT call
            // perform_run() -- that stays in-process and untouched, since
            // it's also what plugin_callbacks.run_project_on_main_thread
            // uses (see below), and that path is synchronous by contract
            // (plugins block on its return value).
            if (studio::EditorTab* active = editor_state.Active(); active && !active->is_welcome) {
                if (terminal_state.run.running.load()) {
                    // A run is already in flight -- same "ignore, don't
                    // queue a second one" behavior perform_run() itself
                    // never had to think about (it always ran to
                    // completion before returning).
                } else if (active->file_path.empty()) {
                    engine.AppendConsoleLine(studio::ConsoleLine::Kind::Error,
                        "save the file (Ctrl+S) before running it -- Run needs a .ava on disk.");
                } else {
                    studio::SaveTab(*active); // ava_cli.exe reads from disk, unlike the old in-process RunScript()
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

        if (want_build) {
            auto& closed = settings.closed_panels;
            auto it = std::find(closed.begin(), closed.end(), "Build");
            if (it != closed.end()) closed.erase(it);
            panel_open["Build"] = true;
            ImGui::SetWindowFocus("Build");
            studio::SaveSettings(settings);
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
        editor_state.close_tab_requested = false;
        editor_state.open_requested = false;
        editor_state.open_folder_requested = false;
        editor_state.new_tab_requested = false;



        if (bool& open = panel_open.try_emplace("Preview", true).first->second; open) {
            if (auto selected = studio::DrawPreviewPanel(demo_tree.root, &open)) {
                properties_state = *selected;
            }
            persist_if_closed("Preview", open);
        }








        if (bool& open = panel_open.try_emplace("Properties", true).first->second; open) {
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
            persist_if_closed("Properties", open);
        }


        if (bool& open = panel_open.try_emplace("Settings", true).first->second; open) {
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
            persist_if_closed("Settings", open);
        }


        if (bool& open = panel_open.try_emplace("Build", true).first->second; open) {
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
                    case studio::BuildBrowseField::kRepoRoot:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_repo_root);
                        break;
                    case studio::BuildBrowseField::kVcpkgRoot:
                        picked = studio::titlebar::OpenFolderDialog(window, path, settings.build_vcpkg_root);
                        break;
                    case studio::BuildBrowseField::kEntryFile:
                        picked = studio::titlebar::OpenFileDialog(
                            window, path,
                            settings.build_project_dir.empty() ? explorer_state.root_dir : settings.build_project_dir);
                        break;
                    case studio::BuildBrowseField::kAvaCliPath:
                        picked = studio::titlebar::OpenFileDialog(window, path, settings.build_ava_cli_path);
                        break;
                    case studio::BuildBrowseField::kKeyFile:
                        picked = studio::titlebar::OpenFileDialog(window, path, explorer_state.root_dir);
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
            persist_if_closed("Build", open);
        }


        if (bool& open = panel_open.try_emplace("Terminal", true).first->second; open) {
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
            persist_if_closed("Terminal", open);
        }


        if (bool& open = panel_open.try_emplace("Output", true).first->second; open) {
            studio::DrawLogsPanel(logs_state, log_bridge, &open);
            persist_if_closed("Output", open);
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
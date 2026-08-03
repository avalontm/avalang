// Ava Studio -- Milestone 1 ("engine first").
//
// A single ImGui window, docked into Explorer / Code Editor / Properties
// / Preview / Terminal / Output, linked directly against the `avalang`
// core library.
// No FFI boundary: this is C++ calling into C++. See studio/CMakeLists.txt
// for the full rationale.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
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
#include "imgui_internal.h" // DockBuilder* -- used once to lay out the default panels on first run

#include "design/avaui_text.h"
#include "design/design_document.h"
#include "engine/engine_bridge.h"
#include "fonts/embedded_font.h"
#include "panels/builtin_panels.h"
#include "panels/editor_panel.h"
#include "panels/explorer_panel.h"
#include "panels/logs_panel.h"
#include "panels/pending_edits_panel.h"
#include "panels/preview_panel.h"
#include "panels/properties_panel.h"
#include "panels/terminal_panel.h"
#include "panels/titlebar_panel.h"
#include "panels/toolbox_panel.h"
#include "palette.h"
#include "plugins/plugin_host.h"
#include "plugins/plugin_ui_bridge.h"
#include "platform/win32_titlebar.h"
#include "theme.h"
#include "util/log_bridge.h"
#include "util/settings.h"

namespace fs = std::filesystem;

namespace {

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Ava Studio's initial workspace folder -- resolved next to the .exe
// (not the process's current working directory, which depends on how
// the app was launched: double-click, a shortcut, a debugger, etc.) so
// "scripts/" is found reliably every time, the same way VSCode always
// opens relative to a known workspace root. Created on first run if it
// doesn't exist yet, so a fresh install has somewhere to save into.
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

// Where PluginHost looks for plugin .dll/.so files -- next to the .exe
// (same base resolution as ResolveWorkspaceDir above, for the same
// reason: independent of the process's working directory), in a
// "plugins" folder created on first run so there's somewhere obvious
// to drop a plugin into. See PLAN_agente_ia_openrouter.md Fase 0.
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

// Height of Ava Studio's own title bar (see panels/titlebar_panel.h) --
// the dockspace below it starts at this offset instead of the very top
// of the window, now that there's no OS-native title bar reserving that
// space for us.
constexpr float kTitleBarHeight = 34.0f;

// Small gap below the custom title bar so the docked panel tabs (Explorer /
// Code Editor / ...) don't sit visually glued to it -- without this, the
// tab row started at y == kTitleBarHeight exactly, right up against the
// titlebar's bottom edge, which read as everything being flush to the top
// of the window with no breathing room.
constexpr float kTitleBarGap = 6.0f;

// Set by GlfwWindowCloseRequested() below -- can't capture state directly
// since GLFW callbacks are plain C function pointers, so this is written
// there and read back once per frame in the main loop instead.
bool g_native_close_requested = false;

// Fires on Alt+F4, the taskbar/dock "close", or any other OS-level close
// request GLFW would otherwise honor immediately. Ava Studio needs a
// chance to warn about unsaved files first (see the exit confirmation in
// main()), so this cancels GLFW's default "close now" and defers to that
// same confirmation instead -- the same one the custom titlebar's X
// button and File > Exit already go through, so every way of closing the
// window behaves consistently.
void GlfwWindowCloseRequested(GLFWwindow* window) {
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    g_native_close_requested = true;
}

} // namespace

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

    // Removes the OS-native title bar (Windows only -- no-op elsewhere)
    // so we can draw our own VSCode-style one below, while keeping
    // native move/resize/Aero-Snap/minimize/maximize. See
    // platform/win32_titlebar.h for how.
    studio::titlebar::Install(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Ava Studio's default font -- JetBrains Mono, compiled into the exe
    // (see src/fonts/). Must happen before the OpenGL3 backend builds its
    // font atlas texture, so this runs before ImGui_ImplOpenGL3_Init() below.
    studio::LoadDefaultFont(16.0f);

    // Bold weight for the Code Editor panel only (see editor_panel.cpp) --
    // loaded into the same atlas, same timing constraint as above.
    studio::LoadBoldFont(16.0f);

    // Persist window/dock layout per-user, the same way VSCode remembers
    // your panel arrangement between sessions -- not next to the exe
    // (which might be a shared/read-only install location) and not in
    // whatever directory ava_studio.exe happens to be launched from.
    // ImGui itself does the actual save/load (on exit and periodically)
    // once io.IniFilename points here; we only need to make sure the
    // folder exists and the string outlives the ImGui context, hence
    // `static` (ImGui keeps the raw pointer, not a copy).
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

    // Settings are loaded once here and applied to the VM immediately --
    // modules_path defaults to "" (first run, or user cleared it), which
    // SetModulesPath() resolves to util::ResolveDefaultModulesDir(). See
    // util/settings.h.
    studio::StudioSettings settings = studio::LoadSettings();
    engine.SetModulesPath(settings.modules_path);

    // One-shot channel from the "Browse..." folder dialog (which can only
    // run here, since it needs `window`) back into the Properties modal's
    // text field a frame later -- see DrawTitleBar's `browsed_folder` param.
    std::string pending_modules_browse;

    studio::ExplorerState explorer_state;
    explorer_state.root_dir = ResolveWorkspaceDir();

    studio::EditorState editor_state;
    studio::InitEditorPanel(editor_state);
    // Same root Explorer is rooted at (explorer_state.root_dir above) --
    // .avaui imports resolve against this, see design/component_resolver.h
    // and EditorState::project_root's comment on why it must be one
    // fixed root shared by the whole recursion.
    editor_state.project_root = explorer_state.root_dir;
    studio::OpenWelcomeTab(editor_state);

    studio::TerminalState terminal_state;
    studio::LogsState logs_state;
    // Separate from `engine`'s console: general Ava Studio logs (plugin
    // load/init, PluginHost edit results) vs. a script's own run output.
    // See util/log_bridge.h for why these are split.
    studio::LogBridge log_bridge;
    studio::PropertiesState properties_state;

    // Build the demo Component Tree once at startup -- see the note in
    // engine_bridge.cpp about why this is fixed rather than script-driven.
    studio::EngineBridge::DemoTree demo_tree = engine.BuildDemoComponentTree();

    // --- Plugin system (Fase 0, see PLAN_agente_ia_openrouter.md) -------
    // Callbacks are lambdas capturing the panel state above by
    // reference rather than PluginHost knowing about ExplorerState/
    // EditorState/TerminalState/EngineBridge directly -- keeps the
    // plugin system decoupled from those types (see plugin_host.h).
    studio::PluginHostCallbacks plugin_callbacks;
    plugin_callbacks.get_project_root = [&]() -> std::string { return explorer_state.root_dir; };
    plugin_callbacks.get_active_file = [&](std::string& path, std::string& content) -> bool {
        const studio::EditorTab* active = editor_state.Active();
        if (!active || active->is_welcome) return false;
        path = active->file_path;
        content = active->GetText();
        return true;
    };
    // Fase 6: the design services (design_add_component/
    // design_edit_component) need the active .avaui tab's CURRENT
    // source, which may live only in tab.design (Design view edits
    // never touch tab.editor) -- so this resolves through
    // WriteAvauiText() when that's the case, same conversion
    // ToggleTabViewMode already does when leaving Design view, rather
    // than reusing get_active_file's raw buffer (which could be stale).
    plugin_callbacks.get_active_avaui_document = [&](std::string& path, std::string& avaui_source) -> bool {
        const studio::EditorTab* active = editor_state.Active();
        if (!active || active->is_welcome || !active->is_avaui) return false;
        path = active->file_path;
        avaui_source = (active->view_mode == studio::TabViewMode::Design)
                           ? studio::design::WriteAvauiText(active->design.root, active->design.code_behind,
                                                             active->design.initial_state, active->design.imports)
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

    // Runs the active tab through the exact same compile+run pipeline the
    // F5 hotkey below uses -- shared by both so run_project() (Fase 5)
    // can't drift from what pressing F5 actually does. Defined here
    // (rather than inline at each call site) since it needs to be handed
    // to PluginHostCallbacks::run_project_on_main_thread, but the F5 key
    // handling further down in the loop calls it too.
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
            // The failing file isn't always the one that was run -- e.g.
            // an error inside an `import`ed module. Falls back to
            // run_source_name when the VM didn't know the file (see
            // RunResult::error_source), same as a top-level error. Open/
            // focus that file's tab first (no-op if it's already the
            // active tab) so HighlightError has something to point at
            // even for a module that was never opened by hand.
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

    // --- Fase 5: write services ------------------------------------------
    plugin_callbacks.write_approved_edit = [&](const studio::PendingEdit& edit) {
        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(fs::path(explorer_state.root_dir) / edit.path, ec);
        if (ec) {
            log_bridge.Log("[plugin_host] no se pudo resolver la ruta al aplicar el cambio: " + edit.path);
            return;
        }

        fs::create_directories(resolved.parent_path(), ec); // no-op if it already exists; ec ignored either way

        std::ofstream out(resolved, std::ios::binary | std::ios::trunc);
        if (!out) {
            log_bridge.Log("[plugin_host] no se pudo escribir el archivo: " + edit.path);
            return;
        }
        out << edit.new_content;
        out.close();

        // If that path is open in a tab, refresh its buffer so it doesn't
        // show stale text -- and clear `dirty` since the buffer now
        // matches what was just written to disk, same as a normal Save
        // would.
        for (const auto& tab_ptr : editor_state.tabs) {
            if (tab_ptr->file_path.empty()) continue;
            std::error_code ec2;
            fs::path tab_path = fs::weakly_canonical(tab_ptr->file_path, ec2);
            if (ec2 || tab_path != resolved) continue;

            tab_ptr->SetText(edit.new_content);
            tab_ptr->dirty = false;

            // Fase 6: an approved edit against a .avaui file can be a
            // design mutation (design_add_component/design_edit_component)
            // as much as a plain text edit -- re-parse into tab.design
            // too, same forgiving policy as SaveTab's Code-view branch
            // (a parse error here just leaves the last valid tab.design
            // untouched; the new text already reached disk and the tab
            // buffer above), so the Design canvas reflects the approved
            // change right away instead of needing a manual F7 round-trip.
            if (tab_ptr->is_avaui) {
                studio::design::DesignNode parsed_root;
                std::string parsed_code_behind;
                std::vector<studio::PropertyRow> parsed_state;
                std::vector<std::string> parsed_imports;
                std::string parse_error;
                if (studio::design::ParseAvauiText(edit.new_content, parsed_root, parsed_code_behind, parsed_state,
                                                    parsed_imports, parse_error)) {
                    tab_ptr->design.root = std::move(parsed_root);
                    tab_ptr->design.code_behind = std::move(parsed_code_behind);
                    tab_ptr->design.initial_state = std::move(parsed_state);
                    tab_ptr->design.imports = std::move(parsed_imports);
                    tab_ptr->design.selected_uid.clear(); // node_uids were reassigned on reparse
                    tab_ptr->design.dirty = false;
                    tab_ptr->avaui_load_error.clear();
                }
            }
            break;
        }

        log_bridge.Log("[plugin_host] cambio aplicado: " + edit.path);
    };
    plugin_callbacks.run_project_on_main_thread = perform_run;

    // Kept around for the lifetime of the loop -- both the initial
    // LoadAll below and every later PluginHost::Reload()/ScanAvailable()
    // triggered by the "Plugins" menu need the same folder.
    const std::string plugins_dir = ResolvePluginsDir();

    studio::PluginHost plugin_host(std::move(plugin_callbacks));
    plugin_host.LoadAll(plugins_dir, settings.disabled_plugins);

    // Per-panel visibility (see the "View" menu, panels/titlebar_panel.cpp,
    // and its `panels`/`closed_panels` params). Keyed by panel name --
    // for a plugin panel that's RegisteredPanel::name (same uniqueness
    // guarantee RegisterPanelTrampoline already enforces); for a
    // built-in panel it's one of the literal names in
    // panels/builtin_panels.h, which must match that panel's own
    // ImGui::Begin() string exactly. A name not yet in the map defaults
    // to open the first time its panel is drawn below -- see the
    // try_emplace calls -- except for names persisted in
    // settings.closed_panels, seeded closed right here so a panel the
    // user closed last session doesn't flash open for one frame before
    // anything reads the settings file.
    std::unordered_map<std::string, bool> panel_open;
    for (const std::string& name : settings.closed_panels) {
        panel_open[name] = false;
    }

    // Persists a panel that was just closed (its p_open flag flipped to
    // false this frame -- via the View menu/Plugins modal's checkbox
    // above, or the panel's own tab "x" button) into
    // settings.closed_panels, so it stays closed across restarts even
    // if the app closes uncleanly. Shared by every built-in and plugin
    // panel draw call below rather than duplicating this
    // find-or-append-and-save logic five-plus times.
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

        // Services any AvaHostServices::run_project() call a plugin's
        // worker thread is currently blocked on -- see PluginHost::
        // PumpMainThreadWork's comment. Deliberately outside the
        // ImGui::NewFrame()/Render() bracket below: perform_run (via
        // plugin_callbacks.run_project_on_main_thread) only touches
        // EditorState/EngineBridge, never ImGui state directly.
        plugin_host.PumpMainThreadWork();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // --- Custom title bar (replaces the OS-native one) -------------
        const bool is_maximized = studio::titlebar::IsWindowMaximizedNow(window);
        // Re-scanned every frame (just a directory listing, see
        // PluginHost::ScanAvailable) so the "Plugins" menu always shows
        // what's actually on disk right now, not a stale snapshot from
        // when the app started.
        const std::vector<studio::PluginInfo> available_plugins =
            plugin_host.ScanAvailable(plugins_dir, settings.disabled_plugins);
        studio::TitleBarResult titlebar_result = studio::DrawTitleBar(
            editor_state, settings, is_maximized, kTitleBarHeight, pending_modules_browse, available_plugins,
            plugin_host.Panels(), settings.closed_panels);
        pending_modules_browse.clear(); // one-shot, consumed by DrawTitleBar this frame

        // "Plugins" menu checkbox toggled this frame -- flip it in
        // settings, persist, and reload right away (no restart needed)
        // before anything below iterates plugin_host.Panels(). Safe to
        // do here: nothing has drawn a plugin panel yet this frame.
        if (!titlebar_result.plugin_toggle_requested.empty()) {
            const std::string& name = titlebar_result.plugin_toggle_requested;
            auto& disabled = settings.disabled_plugins;
            auto it = std::find(disabled.begin(), disabled.end(), name);
            if (it != disabled.end()) {
                disabled.erase(it); // was disabled -> enable it
            } else {
                disabled.push_back(name); // was enabled -> disable it
            }
            studio::SaveSettings(settings);
            plugin_host.Reload(plugins_dir, settings.disabled_plugins);
        }

        // Panel visibility toggled this frame -- either a built-in panel
        // from the "View" menu or a plugin panel from there or the
        // "Plugins" modal's list (see TitleBarResult::panel_toggle_requested).
        // Flip both the persisted list and the runtime map every panel
        // draw call below reads, same as clicking the panel's own tab x
        // would, and persist it right away so it survives even if the
        // app closes uncleanly.
        if (!titlebar_result.panel_toggle_requested.empty()) {
            const std::string& name = titlebar_result.panel_toggle_requested;
            auto& closed = settings.closed_panels;
            auto it = std::find(closed.begin(), closed.end(), name);
            if (it != closed.end()) {
                closed.erase(it); // was closed -> reopen it
                panel_open[name] = true;
                // Bring it to the front of its dock node the moment it
                // reopens -- otherwise it re-docks into its last known
                // spot (imgui.ini remembers that) but silently sits
                // behind whichever tab is already selected there, which
                // looks like nothing happened.
                ImGui::SetWindowFocus(name.c_str());
            } else {
                closed.push_back(name); // was open -> close it
                panel_open[name] = false;
            }
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
        // Every way of asking to quit (this button, File > Exit further
        // down, and native close via the GLFW callback above) funnels
        // into the same "any unsaved changes?" check right before the
        // main loop's end instead of closing here directly -- see
        // want_quit below.
        bool want_quit = titlebar_result.close_clicked;

        // Tells the Win32 hook which pixels are "real" buttons so
        // WM_NCHITTEST doesn't treat clicking them as dragging the
        // window (see platform/win32_titlebar.h). No-op on other
        // platforms.
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
            // While a File/View/Run dropdown is open, treat the *entire*
            // titlebar strip as a real (HTCLIENT) region for this frame
            // instead of just the four button rects. Otherwise a click
            // meant to dismiss the dropdown by clicking elsewhere on the
            // titlebar (the brand icon area, the empty space between
            // buttons, the centered file name, etc.) gets intercepted by
            // Windows as HTCAPTION -- treated as "start dragging the
            // window" -- and never reaches ImGui, so the popup's normal
            // click-outside-closes-it behavior can't fire. Clicks in the
            // dockspace/editor area below the titlebar were never affected
            // by this, since that area is already HTCLIENT.
            extra_rects[4] = studio::titlebar::Rect{
                static_cast<int>(viewport->WorkPos.x), static_cast<int>(viewport->WorkPos.y),
                static_cast<int>(viewport->WorkPos.x + viewport->WorkSize.x),
                static_cast<int>(viewport->WorkPos.y + kTitleBarHeight)};
            extra_rect_count = 5;
        }
        studio::titlebar::UpdateHitRegions(static_cast<int>(kTitleBarHeight), to_rect(titlebar_result.minimize_rect),
                                            to_rect(titlebar_result.maximize_rect), to_rect(titlebar_result.close_rect),
                                            extra_rects, extra_rect_count);

        // --- Dockspace host (everything below the title bar) -----------
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

        // Only runs when this dockspace ID has no saved layout yet --
        // i.e. the very first launch, or if imgui.ini was deleted/moved.
        // Once the user drags a panel anywhere, ImGui's own save (into
        // io.IniFilename) takes over and this block is skipped on every
        // later launch, so their arrangement sticks.
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
            // Sibling tab in the same dock node as Explorer, not its own
            // split -- see 08_DESIGNER_VIEW_PLAN.md section 5.5. Only
            // ever actually drawn (see DrawToolboxPanel() call below)
            // while the active tab is a .avaui in Design view, same as
            // VS6 only showing the Toolbox with a .frm open; docking it
            // here up front just means it lands in the right place
            // instead of floating the first time that happens.
            ImGui::DockBuilderDockWindow("Toolbox", dock_left);
            ImGui::DockBuilderDockWindow("Code Editor", dock_center);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Preview", dock_bottom);
            ImGui::DockBuilderDockWindow("Terminal", dock_bottom); // tabs alongside Preview, like VSCode's bottom panel
            ImGui::DockBuilderDockWindow("Output", dock_bottom);   // general Ava Studio logs -- see util/log_bridge.h

            // Plugin panels (Fase 0) -- docked as a tab alongside
            // whichever built-in panel already occupies their
            // requested slot, same as Toolbox joining Explorer above.
            // Purely a first-run default; the user can drag it
            // anywhere afterward like any other panel.
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

        // --- Explorer -> Editor -----------------------------------------
        // try_emplace seeds "Explorer" open the first time this runs
        // (unless settings.closed_panels already marked it closed above,
        // via the seeding loop right before persist_if_closed) -- same
        // convention the plugin panel loop below uses. `open`'s address
        // becomes Explorer's ImGui::Begin() p_open, so the tab gets a
        // close ("x") button wired to the same flag the View menu's
        // checkbox reads/writes.
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

        // --- Toolbox (only while a .avaui tab is showing Design view) ---
        // Not called at all otherwise -- an ImGui window that isn't
        // drawn on a frame just doesn't appear (it stays docked where
        // DockBuilderDockWindow put it above for next time), same as
        // how Preview/Output already work as always-tabbed-but-
        // sometimes-empty siblings. See 08_DESIGNER_VIEW_PLAN.md
        // section 5.5.
        if (const studio::EditorTab* active = editor_state.Active();
            active && active->is_avaui && active->view_mode == studio::TabViewMode::Design) {
            studio::DrawToolboxPanel();
        }

        // --- Editor -> Save / Run ----------------------------------------
        studio::DrawEditorPanel(editor_state);
        if (const studio::EditorTab* active = editor_state.Active();
            active && active->is_avaui && active->view_mode == studio::TabViewMode::Design) {
            // Unconditional (not `if (designer_selection)`): the active
            // tab's Designer canvas now reports its CURRENT selection
            // every single call (see designer_canvas.cpp's `doc.selected_uid`
            // recompute), including nullopt when nothing is selected in
            // it. Mirroring that unconditionally here is what makes
            // switching to a tab with nothing selected actually clear
            // Properties instead of leaving it pointed at whatever a
            // DIFFERENT tab last had selected -- which, left alone,
            // would let an edit made right now silently patch that
            // OTHER tab's node instead of anything visible on screen.
            properties_state = editor_state.designer_selection ? *editor_state.designer_selection
                                                                 : studio::PropertiesState{};
        }

        // Global hotkeys (checked here, not tied to any single panel's
        // focus, so they work the same whether the click that opened a
        // popup, the Explorer, or the editor currently has focus).
        ImGuiIO& io = ImGui::GetIO();
        const bool want_save    = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S);
        const bool want_save_as = io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S);
        const bool want_new     = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N);
        const bool want_open    = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O);
        const bool want_close_tab = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W);
        const bool want_run     = ImGui::IsKeyPressed(ImGuiKey_F5);
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
                // Switches the Explorer panel over to the chosen directory,
                // the same "open a project" role VSCode's Open Folder plays --
                // it's just the working directory Explorer browses from,
                // it doesn't touch whatever tabs are already open in the
                // Code Editor.
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
                // Untitled buffer: Ctrl+S / File > Save falls through to the
                // Save As dialog, same as most editors.
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
            // No-op for any tab that isn't .avaui -- ToggleTabViewMode
            // guards on tab.is_avaui itself, same as VS6 where F7 only
            // meant something with a .frm open.
            if (studio::EditorTab* active = editor_state.Active()) {
                studio::ToggleTabViewMode(*active);
            }
        }
        if (editor_state.run_requested || want_run) {
            // Discards the RunProjectResult here -- terminal_state was
            // already updated as a side effect (same as before this was
            // factored out into perform_run), which is all this call site
            // ever needed. See perform_run's definition above, right
            // after plugin_callbacks is built, for the Fase 5 caller that
            // actually uses the return value.
            perform_run();
        }
        if (titlebar_result.modules_browse_requested) {
            std::string path;
            if (studio::titlebar::OpenFolderDialog(window, path, settings.modules_path)) {
                pending_modules_browse = path;
            }
        }
        if (titlebar_result.modules_save_requested) {
            engine.SetModulesPath(settings.modules_path);
            studio::SaveSettings(settings);
        }
        // Fold in File > Exit and any native close request (Alt+F4, the
        // taskbar/dock close, etc. -- see GlfwWindowCloseRequested) so
        // every path to quitting resolves through the one check below.
        want_quit = want_quit || titlebar_result.quit_requested || g_native_close_requested;
        g_native_close_requested = false; // consumed this frame either way

        if (want_quit) {
            if (studio::HasUnsavedChanges(editor_state)) {
                ImGui::OpenPopup("Unsaved Changes##ExitConfirm");
            } else {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        // --- Unsaved-changes confirmation on exit (Save All & Exit /
        // Exit / Cancel) -- drawn every frame so it stays open across
        // frames once OpenPopup fires above, same pattern as every other
        // modal in this file/the panels.
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

            // Three outcomes, three colors, so the button someone taps
            // under pressure matches what they meant: green = keeps your
            // work, red = throws it away, gray = does nothing / backs out.
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float button_w = (ImGui::GetContentRegionAvail().x - 2.0f * spacing) / 3.0f;

            ImGui::PushStyleColor(ImGuiCol_Button, studio::palette::FromHex(studio::palette::kSuccess, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, studio::palette::FromHex(studio::palette::kSuccess, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, studio::palette::FromHex(0x189b48));
            if (ImGui::Button("Save All & Exit", ImVec2(button_w, 0.0f))) {
                studio::SaveAllTabs(editor_state);
                // SaveAllTabs() skips untitled tabs (nothing to write into
                // without a path) -- walk those here, one Save As dialog
                // at a time, same as Ctrl+S on an untitled buffer would.
                // If the user cancels any of those dialogs, stay open
                // instead of quitting so that tab's work isn't discarded.
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
            // Renamed from "Exit" -- the label now says exactly what it
            // does (discards unsaved work) instead of leaving that to be
            // inferred from position/color alone.
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

        // --- Preview -> Properties -----------------------------------------
        // Same try_emplace-seeds-open convention as Explorer above.
        if (bool& open = panel_open.try_emplace("Preview", true).first->second; open) {
            if (auto selected = studio::DrawPreviewPanel(demo_tree.root, &open)) {
                properties_state = *selected;
            }
            persist_if_closed("Preview", open);
        }
        // Fase 3 (write-back): DrawPropertiesPanel edits properties_state's
        // own PropertyRow::value in place as the person types (so the table
        // shows keystrokes immediately, see properties_panel.cpp), and
        // returns a PropertyEdit only once a field is committed
        // (unfocus/Enter) -- that's the point this patches the *real*
        // DesignNode the selection came from, identified by tab id + uid
        // rather than a stale pointer (properties_state outlives whatever
        // frame the selection was made on, including tab switches).
        if (bool& open = panel_open.try_emplace("Properties", true).first->second; open) {
            if (auto edit = studio::DrawPropertiesPanel(properties_state, &open)) {
                for (auto& tab_ptr : editor_state.tabs) {
                studio::EditorTab& tab = *tab_ptr;
                if (tab.id != edit->tab_id || !tab.is_avaui) continue;
                if (studio::design::DesignNode* node =
                        studio::design::FindNodeByUid(tab.design.root, edit->node_uid)) {
                    // 9.9/9.12: every kind of edit the Properties panel
                    // can now emit -- see PropertyEditKind in
                    // properties_panel.h for what each one means and
                    // which field(s) of PropertyEdit it uses.
                    switch (edit->kind) {
                        case studio::PropertyEditKind::kValue:
                            for (auto& prop : node->properties) {
                                if (prop.key == edit->key) {
                                    prop.value = edit->new_value;
                                    break;
                                }
                            }
                            break;
                        case studio::PropertyEditKind::kId:
                            node->id = edit->new_value;
                            break;
                        case studio::PropertyEditKind::kType:
                            node->type = edit->new_value;
                            break;
                        case studio::PropertyEditKind::kAddProperty:
                            // Guard against a duplicate key slipping in
                            // anyway (e.g. two edits committed the same
                            // frame from a stale UI state) -- the panel
                            // already disables its own "+" button for a
                            // key that exists, this is just a second,
                            // cheap line of defense against a
                            // duplicate that would otherwise be
                            // impossible to tell apart in the table.
                            {
                                bool exists = false;
                                for (const auto& prop : node->properties) {
                                    if (prop.key == edit->key) { exists = true; break; }
                                }
                                if (!exists) node->properties.push_back({edit->key, edit->new_value});
                            }
                            break;
                        case studio::PropertyEditKind::kRemoveProperty:
                            for (auto it = node->properties.begin(); it != node->properties.end(); ++it) {
                                if (it->key == edit->key) {
                                    node->properties.erase(it);
                                    break;
                                }
                            }
                            break;
                        case studio::PropertyEditKind::kEvent: {
                            // Shared by "value changed" and "new row
                            // added" (see properties_panel.cpp's
                            // DrawEditableRowTable call for events) --
                            // find-or-add covers both: an existing
                            // event's handler gets updated in place, a
                            // brand-new event name gets appended.
                            bool found = false;
                            for (auto& ev : node->events) {
                                if (ev.key == edit->key) {
                                    ev.value = edit->new_value;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) node->events.push_back({edit->key, edit->new_value});
                            break;
                        }
                        case studio::PropertyEditKind::kRemoveEvent:
                            for (auto it = node->events.begin(); it != node->events.end(); ++it) {
                                if (it->key == edit->key) {
                                    node->events.erase(it);
                                    break;
                                }
                            }
                            break;
                    }
                    // Same convention SaveTab/HandleDropTarget already use:
                    // this loop sets design-doc dirty, DrawEditorPanel's
                    // "if (tab.design.dirty) tab.dirty = true" line only
                    // runs on its own next call, so mirror it here too --
                    // otherwise the tab strip's unsaved dot wouldn't light
                    // up until the next click on the canvas.
                    tab.design.dirty = true;
                    tab.dirty = true;
                }
                // Tab ids are unique (EditorState::next_tab_id only ever
                // increments) -- no need to keep scanning once matched.
                break;
            }
            }
            persist_if_closed("Properties", open);
        }

        // --- Terminal --------------------------------------------------------
        if (bool& open = panel_open.try_emplace("Terminal", true).first->second; open) {
            if (auto file_click = studio::DrawTerminalPanel(terminal_state, engine, &open)) {
                // Clicking an older Error line in the scrollback jumps to it
                // again, exactly like the auto-open/highlight right after a
                // failed Run above -- open/focus the file (no-op if it's
                // already the active tab) and highlight the position.
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

        // --- Output (general logs) ------------------------------------------
        if (bool& open = panel_open.try_emplace("Output", true).first->second; open) {
            studio::DrawLogsPanel(logs_state, log_bridge, &open);
            persist_if_closed("Output", open);
        }

        // --- Plugin panels (Fase 0) -----------------------------------
        // Tabbed alongside the built-in panels (same as Preview/Terminal/
        // Output) -- a plugin has no concept of "only visible in Design
        // view" the way Toolbox does, so there's no view-mode conditional
        // here. Same closable convention the built-ins above now share
        // too (see panel_open's comment): try_emplace seeds a
        // first-seen panel open (unless settings.closed_panels already
        // marked it closed above), then Begin's p_open both draws the
        // tab's x button and -- once the user clicks it -- flips `open`
        // to false, so next frame this loop simply stops calling
        // Begin() for it. ImGui itself remembers that panel's last dock
        // location (imgui.ini, keyed by its window name) for whenever
        // `open` goes back to true, either via the View menu, the
        // "Plugins" modal's panel list, or a future ReopenPanel menu --
        // no manual re-docking needed here.
        for (const auto& panel : plugin_host.Panels()) {
            bool& open = panel_open.try_emplace(panel.name, true).first->second;
            if (!open) continue;

            ImGui::Begin(panel.name.c_str(), &open);
            AvaPanelContext* ctx = studio::plugins_ui::BeginPanelContext(panel.name.c_str());
            panel.draw(ctx, panel.user_data);
            studio::plugins_ui::EndPanelContext(ctx);
            ImGui::End();

            // The x button just set `open` to false above -- persist
            // that immediately (same as the View menu's / Plugins
            // modal's checkbox does) so a panel closed by its tab, not
            // just one of those menus, also stays closed across
            // restarts.
            persist_if_closed(panel.name, open);
        }

        // --- Fase 5: apply_edit approval gate --------------------------
        // Draws nothing when there are no pending proposals (see its own
        // comment) -- called every frame, after plugins have had a chance
        // to queue one via AvaHostServices::apply_edit this frame, so a
        // proposal shows up the same frame it was made rather than a
        // frame late.
        studio::DrawPendingEditsPanel(plugin_host);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.043f, 0.043f, 0.051f, 1.0f); // #0b0b0d, matches theme.cpp's bg_editor
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Every loaded plugin's ava_plugin_shutdown() runs (reverse load
    // order) while the ImGui/GLFW context it drew into is still alive --
    // a plugin has no business touching either after this point, but
    // tearing them down before the context itself is destroyed keeps
    // that boundary clean either way.
    plugin_host.UnloadAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
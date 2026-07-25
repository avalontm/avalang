// Ava Studio -- Milestone 1 ("engine first").
//
// A single ImGui window, docked into Explorer / Code Editor / Properties
// / Preview / Output, linked directly against the `avalang` core library.
// No FFI boundary: this is C++ calling into C++. See studio/CMakeLists.txt
// for the full rationale.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

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

#include "engine/engine_bridge.h"
#include "fonts/embedded_font.h"
#include "panels/editor_panel.h"
#include "panels/explorer_panel.h"
#include "panels/output_panel.h"
#include "panels/preview_panel.h"
#include "panels/properties_panel.h"
#include "panels/titlebar_panel.h"
#include "panels/toolbox_panel.h"
#include "palette.h"
#include "platform/win32_titlebar.h"
#include "theme.h"
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

    studio::OutputState output_state;
    studio::PropertiesState properties_state;

    // Build the demo Component Tree once at startup -- see the note in
    // engine_bridge.cpp about why this is fixed rather than script-driven.
    studio::EngineBridge::DemoTree demo_tree = engine.BuildDemoComponentTree();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // --- Custom title bar (replaces the OS-native one) -------------
        const bool is_maximized = studio::titlebar::IsWindowMaximizedNow(window);
        studio::TitleBarResult titlebar_result =
            studio::DrawTitleBar(editor_state, settings, is_maximized, kTitleBarHeight, pending_modules_browse);
        pending_modules_browse.clear(); // one-shot, consumed by DrawTitleBar this frame

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
        studio::titlebar::Rect extra_rects[4] = {
            to_rect(titlebar_result.file_menu_rect),
            to_rect(titlebar_result.run_menu_rect),
            to_rect(titlebar_result.about_rect),
        };
        int extra_rect_count = 3;
        if (titlebar_result.any_popup_open) {
            // While a File/Run dropdown is open, treat the *entire*
            // titlebar strip as a real (HTCLIENT) region for this frame
            // instead of just the three button rects. Otherwise a click
            // meant to dismiss the dropdown by clicking elsewhere on the
            // titlebar (the brand icon area, the empty space between
            // buttons, the centered file name, etc.) gets intercepted by
            // Windows as HTCAPTION -- treated as "start dragging the
            // window" -- and never reaches ImGui, so the popup's normal
            // click-outside-closes-it behavior can't fire. Clicks in the
            // dockspace/editor area below the titlebar were never affected
            // by this, since that area is already HTCLIENT.
            extra_rects[3] = studio::titlebar::Rect{
                static_cast<int>(viewport->WorkPos.x), static_cast<int>(viewport->WorkPos.y),
                static_cast<int>(viewport->WorkPos.x + viewport->WorkSize.x),
                static_cast<int>(viewport->WorkPos.y + kTitleBarHeight)};
            extra_rect_count = 4;
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
            ImGui::DockBuilderDockWindow("Output", dock_bottom); // tabs alongside Preview, like VSCode's bottom panel

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
        ImGui::End();

        // --- Explorer -> Editor -----------------------------------------
        studio::ExplorerResult explorer_result = studio::DrawExplorerPanel(explorer_state);
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
        if (editor_state.designer_selection) {
            properties_state = *editor_state.designer_selection;
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
            if (const studio::EditorTab* active = editor_state.Active(); active && !active->is_welcome) {
                studio::ClearErrorHighlights(editor_state);
                output_state.last_run = engine.RunScript(active->GetText(), active->file_path);
                output_state.has_run_result = true;
                if (!output_state.last_run.success) {
                    studio::HighlightError(editor_state, active->file_path,
                                            output_state.last_run.error_line,
                                            output_state.last_run.error_column,
                                            output_state.last_run.message);
                }
            }
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
        if (auto selected = studio::DrawPreviewPanel(demo_tree.root)) {
            properties_state = *selected;
        }
        studio::DrawPropertiesPanel(properties_state);

        // --- Output --------------------------------------------------------
        studio::DrawOutputPanel(output_state, engine);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.043f, 0.043f, 0.051f, 1.0f); // #0b0b0d, matches theme.cpp's bg_editor
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
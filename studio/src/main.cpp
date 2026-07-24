// Ava Studio -- Milestone 1 ("engine first").
//
// A single ImGui window, docked into Explorer / Code Editor / Properties
// / Preview / Output, linked directly against the `avalang` core library.
// No FFI boundary: this is C++ calling into C++. See studio/CMakeLists.txt
// for the full rationale.

#include <cstdio>

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "engine/engine_bridge.h"
#include "panels/editor_panel.h"
#include "panels/explorer_panel.h"
#include "panels/output_panel.h"
#include "panels/preview_panel.h"
#include "panels/properties_panel.h"

namespace {

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    studio::EngineBridge engine;

    studio::ExplorerState explorer_state;
    explorer_state.root_dir = "scripts"; // relative to the working directory ava_studio is launched from

    studio::EditorState editor_state;
    editor_state.text =
        "func greet(name)\n"
        "    print($\"Hello {name}!\")\n"
        "end\n"
        "\n"
        "greet(\"Ava Studio\")\n";

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
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("AvaStudioDockHost", nullptr, host_flags);
        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save", "Ctrl+S")) editor_state.save_requested = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Run")) {
                if (ImGui::MenuItem("Run Script", "F5")) editor_state.run_requested = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGuiID dockspace_id = ImGui::GetID("AvaStudioDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
        ImGui::End();

        // --- Explorer -> Editor -----------------------------------------
        if (auto clicked_file = studio::DrawExplorerPanel(explorer_state)) {
            studio::LoadFileIntoEditor(editor_state, *clicked_file);
        }

        // --- Editor -> Save / Run ----------------------------------------
        studio::DrawEditorPanel(editor_state);
        if (editor_state.save_requested) {
            studio::SaveEditorToFile(editor_state);
        }
        if (editor_state.run_requested) {
            output_state.last_run = engine.RunScript(editor_state.text, editor_state.file_path);
            output_state.has_run_result = true;
        }

        // --- Preview -> Properties -----------------------------------------
        if (auto selected = studio::DrawPreviewPanel(demo_tree.root)) {
            properties_state = *selected;
        }
        studio::DrawPropertiesPanel(properties_state);

        // --- Output --------------------------------------------------------
        output_state.last_tree_json = demo_tree.json;
        studio::DrawOutputPanel(output_state);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.11f, 1.0f);
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

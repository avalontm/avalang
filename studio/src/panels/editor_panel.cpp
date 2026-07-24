#include "panels/editor_panel.h"

#include <fstream>
#include <sstream>

#include "imgui.h"

namespace studio {

void LoadFileIntoEditor(EditorState& state, const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return;
    std::ostringstream ss;
    ss << file.rdbuf();
    state.file_path = path;
    state.text = ss.str();
    state.dirty = false;
}

void SaveEditorToFile(EditorState& state) {
    if (state.file_path.empty()) return;
    std::ofstream file(state.file_path, std::ios::binary);
    if (!file) return;
    file << state.text;
    state.dirty = false;
}

namespace {

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

} // namespace

void DrawEditorPanel(EditorState& state) {
    state.run_requested = false;
    state.save_requested = false;

    ImGui::Begin("Code Editor");

    const std::string title = state.file_path.empty() ? "(sin archivo)" : state.file_path;
    ImGui::TextDisabled("%s%s", title.c_str(), state.dirty ? " *" : "");

    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
    if (ImGui::Button("Save (Ctrl+S)")) {
        state.save_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Run (F5)")) {
        state.run_requested = true;
    }

    ImGui::Separator();

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) state.save_requested = true;
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) state.run_requested = true;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    bool changed = ImGui::InputTextMultiline(
        "##editor",
        state.text.data(),
        state.text.size() + 1, // +1 so an empty buffer still has room for the callback resize
        avail,
        ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize,
        InputTextCallback,
        &state.text
    );
    if (changed) state.dirty = true;

    ImGui::End();
}

} // namespace studio

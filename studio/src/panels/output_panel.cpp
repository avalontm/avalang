#include "panels/output_panel.h"

#include "imgui.h"

namespace studio {

void DrawOutputPanel(const OutputState& state) {
    ImGui::Begin("Output");

    if (!state.has_run_result) {
        ImGui::TextDisabled("Presiona Run (F5) para compilar y correr el script del editor.");
    } else if (state.last_run.success) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", state.last_run.message.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "%s", state.last_run.message.c_str());
    }

    if (!state.last_tree_json.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Component Tree JSON (ava_ui_tree_to_json):");
        ImGui::BeginChild("tree_json", ImVec2(0, 0), true);
        ImGui::TextUnformatted(state.last_tree_json.c_str());
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace studio

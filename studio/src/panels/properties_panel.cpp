#include "panels/properties_panel.h"

#include "imgui.h"

namespace studio {

void DrawPropertiesPanel(const PropertiesState& state) {
    ImGui::Begin("Properties");

    if (state.selected_component_type.empty()) {
        ImGui::TextDisabled("Selecciona un componente en Preview.");
        ImGui::End();
        return;
    }

    ImGui::Text("Type: %s", state.selected_component_type.c_str());
    if (!state.selected_component_id.empty()) {
        ImGui::Text("Id: %s", state.selected_component_id.c_str());
    }
    ImGui::Separator();

    if (ImGui::BeginTable("props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        for (const auto& row : state.properties) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.key.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.value.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace studio

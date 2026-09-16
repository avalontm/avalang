#include "panels/state_panel.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "util/i18n.h"

namespace studio {

namespace {

bool DrawRemoveButton(const char* str_id) {
    ImGui::PushID(str_id);
    const bool clicked = ImGui::SmallButton("x");
    ImGui::PopID();
    return clicked;
}

}

std::optional<StateEdit> DrawStatePanel(StateEditorState& state, bool* p_open) {
    std::optional<StateEdit> committed;

    static std::string add_key_buffer;
    static std::string rename_buffer;
    static int renaming_index = -1;

    const std::string title = util::Tr("panel.state.title") + "###state";
    ImGui::Begin(title.c_str(), p_open);

    if (!state.has_code_behind) {
        ImGui::TextDisabled("%s", util::Tr("state.empty_selection").c_str());
        ImGui::End();
        return committed;
    }

    ImGui::TextUnformatted(util::Tr("state.section_variables").c_str());
    ImGui::Separator();

    if (ImGui::BeginTable("state_vars", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(util::Tr("state.column_name").c_str());
        ImGui::TableSetupColumn(util::Tr("state.column_initial").c_str());
        ImGui::TableSetupColumn(util::Tr("state.column_evaluated").c_str());
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        int remove_index = -1;

        for (int i = 0; i < static_cast<int>(state.variables.size()); ++i) {
            StateVarRow& row = state.variables[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (renaming_index == i) {
                ImGui::InputText("##rename", &rename_buffer);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!rename_buffer.empty() && rename_buffer != row.key) {
                        committed = StateEdit{state.source_tab_id, StateEditKind::kRenameVariable, row.key,
                                               rename_buffer, row.value};
                    }
                    renaming_index = -1;
                }
                if (ImGui::IsItemDeactivated() && rename_buffer == row.key) {
                    renaming_index = -1;
                }
            } else {
                ImGui::TextUnformatted(row.key.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    renaming_index = i;
                    rename_buffer = row.key;
                }
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##value", &row.value);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                committed = StateEdit{state.source_tab_id, StateEditKind::kValue, row.key, row.key, row.value};
            }

            ImGui::TableSetColumnIndex(2);
            if (row.has_error) {
                ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.0f), "%s", row.evaluated.c_str());
            } else {
                ImGui::TextDisabled("%s", row.evaluated.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            if (DrawRemoveButton("##remove_row")) {
                remove_index = i;
            }

            ImGui::PopID();
        }

        if (remove_index >= 0) {
            committed = StateEdit{state.source_tab_id, StateEditKind::kRemoveVariable,
                                   state.variables[remove_index].key, "", ""};
            state.variables.erase(state.variables.begin() + remove_index);
            if (renaming_index == remove_index) renaming_index = -1;
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##add_key", util::Tr("state.new_variable_hint").c_str(), &add_key_buffer);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("--");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("--");
        ImGui::TableSetColumnIndex(3);

        bool key_taken = false;
        for (const StateVarRow& row : state.variables) {
            if (row.key == add_key_buffer) { key_taken = true; break; }
        }
        const bool can_add = !add_key_buffer.empty() && !key_taken;
        ImGui::BeginDisabled(!can_add);
        if (ImGui::SmallButton("+")) {
            committed = StateEdit{state.source_tab_id, StateEditKind::kAddVariable, add_key_buffer,
                                   add_key_buffer, ""};
            state.variables.push_back(StateVarRow{add_key_buffer, "", "", false});
            add_key_buffer.clear();
        }
        ImGui::EndDisabled();

        ImGui::EndTable();
    }

    ImGui::End();
    return committed;
}

}

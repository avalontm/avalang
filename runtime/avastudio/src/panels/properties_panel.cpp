#include "panels/properties_panel.h"

#include "design/component_catalog.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::string TrFormat(const std::string& key, const std::string& arg) {
    std::string result = util::Tr(key);
    const size_t pos = result.find("%s");
    if (pos == std::string::npos) return result;
    return result.substr(0, pos) + arg + result.substr(pos + 2);
}

bool DrawRemoveButton(const char* str_id) {
    ImGui::PushID(str_id);
    const bool clicked = ImGui::SmallButton("x");
    ImGui::PopID();
    return clicked;
}

std::optional<PropertyEdit> DrawEditableRowTable(const char* table_id, std::vector<PropertyRow>& rows,
                                                  std::string& add_key_buffer, int tab_id,
                                                  const std::string& node_uid, PropertyEditKind value_kind,
                                                  PropertyEditKind add_kind, PropertyEditKind remove_kind) {
    std::optional<PropertyEdit> committed;

    if (ImGui::BeginTable(table_id, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(util::Tr("properties.column_key").c_str());
        ImGui::TableSetupColumn(util::Tr("properties.column_value").c_str());
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        int remove_index = -1;

        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            PropertyRow& row = rows[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.key.c_str());
            ImGui::TableSetColumnIndex(1);

            ImGui::PushID(i);
            ImGui::SetNextItemWidth(-FLT_MIN);

            ImGui::InputText("##value", &row.value);

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                committed = PropertyEdit{tab_id, node_uid, value_kind, row.key, row.value};
            }
            ImGui::TableSetColumnIndex(2);
            if (DrawRemoveButton("##remove_row")) {
                remove_index = i;
            }
            ImGui::PopID();
        }

        if (remove_index >= 0) {
            committed = PropertyEdit{tab_id, node_uid, remove_kind, rows[remove_index].key, ""};

            rows.erase(rows.begin() + remove_index);
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::PushID("##add_key");
        ImGui::InputTextWithHint("##add_key", util::Tr("properties.new_key_hint").c_str(), &add_key_buffer);
        ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("--");
        ImGui::TableSetColumnIndex(2);

        bool key_taken = false;
        for (const PropertyRow& row : rows) {
            if (row.key == add_key_buffer) { key_taken = true; break; }
        }
        const bool can_add = !add_key_buffer.empty() && !key_taken;
        ImGui::BeginDisabled(!can_add);
        if (ImGui::SmallButton("+")) {
            committed = PropertyEdit{tab_id, node_uid, add_kind, add_key_buffer, ""};
            rows.push_back(PropertyRow{add_key_buffer, ""});
            add_key_buffer.clear();
        }
        ImGui::EndDisabled();

        ImGui::EndTable();
    }

    return committed;
}

}

std::optional<PropertyEdit> DrawPropertiesPanel(PropertiesState& state, bool* p_open) {
    std::optional<PropertyEdit> committed;

    static std::string add_property_key;
    static std::string add_event_key;

    const std::string title = util::Tr("panel.properties.title") + "###properties";
    ImGui::Begin(title.c_str(), p_open);

    if (state.selected_component_type.empty()) {
        ImGui::TextDisabled("%s", util::Tr("properties.empty_selection").c_str());
        ImGui::End();
        return committed;
    }

    if (state.editable) {

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::TextUnformatted(util::Tr("properties.type_label").c_str());
        if (ImGui::BeginCombo("##type_combo", state.selected_component_type.c_str())) {
            for (const design::ComponentTypeInfo& info : design::GetComponentCatalog()) {
                const bool is_selected = (info.type == state.selected_component_type);
                if (ImGui::Selectable(info.display_name.c_str(), is_selected)) {
                    if (info.type != state.selected_component_type) {
                        committed = PropertyEdit{state.source_tab_id, state.selected_node_id,
                                                  PropertyEditKind::kType, "", info.type};
                        state.selected_component_type = info.type;
                    }
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TextUnformatted(util::Tr("properties.id_label").c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##id_value", &state.selected_component_id);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            committed = PropertyEdit{state.source_tab_id, state.selected_node_id, PropertyEditKind::kId, "",
                                      state.selected_component_id};
        }
    } else {
        ImGui::Text("%s", TrFormat("properties.type_display", state.selected_component_type).c_str());
        if (!state.selected_component_id.empty()) {
            ImGui::Text("%s", TrFormat("properties.id_display", state.selected_component_id).c_str());
        }

        ImGui::TextDisabled("%s", util::Tr("properties.readonly_note").c_str());
    }
    ImGui::Separator();

    ImGui::TextUnformatted(util::Tr("panel.properties.title").c_str());
    if (state.editable) {
        if (auto edit = DrawEditableRowTable("props", state.properties, add_property_key,
                                              state.source_tab_id, state.selected_node_id,
                                              PropertyEditKind::kValue, PropertyEditKind::kAddProperty,
                                              PropertyEditKind::kRemoveProperty)) {
            committed = edit;
        }
    } else if (ImGui::BeginTable("props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(util::Tr("properties.column_property").c_str());
        ImGui::TableSetupColumn(util::Tr("properties.column_value").c_str());
        ImGui::TableHeadersRow();
        for (const PropertyRow& row : state.properties) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.key.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.value.c_str());
        }
        ImGui::EndTable();
    }

    if (state.editable || !state.events.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted(util::Tr("properties.section_events").c_str());
        if (state.editable) {
            if (auto edit = DrawEditableRowTable("events", state.events, add_event_key, state.source_tab_id,
                                                  state.selected_node_id, PropertyEditKind::kEvent,
                                                  PropertyEditKind::kEvent, PropertyEditKind::kRemoveEvent)) {
                committed = edit;
            }
        } else if (ImGui::BeginTable("events", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(util::Tr("properties.column_event").c_str());
            ImGui::TableSetupColumn(util::Tr("properties.column_handler").c_str());
            ImGui::TableHeadersRow();
            for (const PropertyRow& row : state.events) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.key.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.value.c_str());
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
    return committed;
}

}

#include "panels/properties_panel.h"

#include <cfloat>
#include <regex>
#include <sstream>

#include "design/component_catalog.h"
#include "designer/property_editor.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "util/i18n.h"

namespace studio {

namespace {

const char* CategoryTrKey(designer::PropertyCategory category) {
    switch (category) {
        case designer::PropertyCategory::Identity: return "properties.category.identity";
        case designer::PropertyCategory::Layout: return "properties.category.layout";
        case designer::PropertyCategory::Appearance: return "properties.category.appearance";
        case designer::PropertyCategory::Typography: return "properties.category.typography";
        case designer::PropertyCategory::Behavior: return "properties.category.behavior";
        case designer::PropertyCategory::Accessibility: return "properties.category.accessibility";
        case designer::PropertyCategory::Events: return "properties.category.events";
        case designer::PropertyCategory::Advanced: return "properties.category.advanced";
    }
    return "properties.category.advanced";
}

const char* SourceTrKey(designer::PropertySource source) {
    switch (source) {
        case designer::PropertySource::Local: return "properties.source.local";
        case designer::PropertySource::Style: return "properties.source.style";
        case designer::PropertySource::Theme: return "properties.source.theme";
        case designer::PropertySource::Inherited: return "properties.source.inherited";
        case designer::PropertySource::Default: return "properties.source.default";
    }
    return "properties.source.default";
}

std::vector<std::string> ExtractRegexAlternatives(const std::string& validation) {
    std::vector<std::string> alternatives;
    std::smatch match;
    static const std::regex kAlternationPattern(R"(\(([a-zA-Z0-9_|\-]+)\))");
    if (!std::regex_search(validation, match, kAlternationPattern) || match.size() < 2) {
        return alternatives;
    }
    std::string group = match[1].str();
    std::string current;
    for (char c : group) {
        if (c == '|') {
            if (!current.empty()) alternatives.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) alternatives.push_back(current);
    return alternatives;
}

bool DrawRemoveButton(const char* str_id) {
    ImGui::PushID(str_id);
    const bool clicked = ImGui::SmallButton("x");
    ImGui::PopID();
    return clicked;
}

void DrawSourceTag(designer::PropertySource source) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", util::Tr(SourceTrKey(source)).c_str());
}

std::optional<PropertyEdit> DrawColorRow(designer::PropertyGridRow& row, int tab_id, const std::string& node_uid) {
    std::optional<PropertyEdit> committed;
    float rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    designer::TryParseColorValue(row.value, rgba);

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::ColorEdit4("##value", rgba, ImGuiColorEditFlags_AlphaBar)) {
        row.value = designer::FormatColorValue(rgba);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
    }
    return committed;
}

std::optional<PropertyEdit> DrawBooleanRow(designer::PropertyGridRow& row, int tab_id,
                                            const std::string& node_uid) {
    std::optional<PropertyEdit> committed;
    bool checked = (row.value == "true");
    if (ImGui::Checkbox("##value", &checked)) {
        row.value = checked ? "true" : "false";
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
    }
    return committed;
}

std::optional<PropertyEdit> DrawNumberRow(designer::PropertyGridRow& row, int tab_id,
                                           const std::string& node_uid) {
    std::optional<PropertyEdit> committed;
    double number = 0.0;
    try {
        number = row.value.empty() ? 0.0 : std::stod(row.value);
    } catch (...) {
        number = 0.0;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputDouble("##value", &number, 0.0, 0.0, "%.6g");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::ostringstream out;
        out << number;
        row.value = out.str();
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
    }
    return committed;
}

std::optional<PropertyEdit> DrawEnumRow(designer::PropertyGridRow& row, int tab_id, const std::string& node_uid) {
    std::optional<PropertyEdit> committed;
    const std::vector<std::string> options = ExtractRegexAlternatives(row.metadata.validation);
    if (options.empty()) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##value", &row.value);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("properties.enum_no_options_hint").c_str());
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
        }
        return committed;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", row.value.c_str())) {
        for (const std::string& option : options) {
            const bool is_selected = (option == row.value);
            if (ImGui::Selectable(option.c_str(), is_selected)) {
                if (option != row.value) {
                    row.value = option;
                    committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, option};
                }
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return committed;
}

std::optional<PropertyEdit> DrawTextRow(designer::PropertyGridRow& row, int tab_id, const std::string& node_uid) {
    std::optional<PropertyEdit> committed;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##value", &row.value);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
    }
    return committed;
}

std::optional<PropertyEdit> DrawGridRowEditor(designer::PropertyGridRow& row, int tab_id,
                                               const std::string& node_uid) {
    switch (row.metadata.designerEditor) {
        case designer::PropertyEditorKind::Color:
            return DrawColorRow(row, tab_id, node_uid);
        case designer::PropertyEditorKind::Boolean:
            return DrawBooleanRow(row, tab_id, node_uid);
        case designer::PropertyEditorKind::Number:
        case designer::PropertyEditorKind::Dimension:
            return DrawNumberRow(row, tab_id, node_uid);
        case designer::PropertyEditorKind::Enum:
            return DrawEnumRow(row, tab_id, node_uid);
        case designer::PropertyEditorKind::String:
        case designer::PropertyEditorKind::Font:
        case designer::PropertyEditorKind::Padding:
        case designer::PropertyEditorKind::Margin:
        case designer::PropertyEditorKind::Resource:
        case designer::PropertyEditorKind::Binding:
        case designer::PropertyEditorKind::Event:
        case designer::PropertyEditorKind::Style:
        default:
            return DrawTextRow(row, tab_id, node_uid);
    }
}

std::optional<PropertyEdit> DrawPropertyGridSection(designer::PropertyGridSection& section, int tab_id,
                                                     const std::string& node_uid) {
    std::optional<PropertyEdit> committed;

    const std::string header = util::Tr(CategoryTrKey(section.category));
    if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        return committed;
    }

    ImGui::PushID(header.c_str());
    if (ImGui::BeginTable("grid", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(util::Tr("properties.column_key").c_str());
        ImGui::TableSetupColumn(util::Tr("properties.column_value").c_str());
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        for (designer::PropertyGridRow& row : section.rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.metadata.name.c_str());
            if (!row.metadata.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", row.metadata.description.c_str());
            }
            ImGui::TableSetColumnIndex(1);

            ImGui::PushID(row.metadata.name.c_str());
            if (row.metadata.readOnly) {
                ImGui::TextUnformatted(row.value.c_str());
            } else {
                if (auto edit = DrawGridRowEditor(row, tab_id, node_uid)) {
                    committed = edit;
                }
            }
            DrawSourceTag(row.source);

            ImGui::TableSetColumnIndex(2);
            if (row.source == designer::PropertySource::Local && !row.metadata.readOnly) {
                if (DrawRemoveButton("##remove_override")) {
                    committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kRemoveProperty,
                                              row.metadata.name, ""};
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", util::Tr("properties.remove_override_tooltip").c_str());
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    ImGui::PopID();

    return committed;
}

std::optional<PropertyEdit> DrawAddCustomPropertyRow(int tab_id, const std::string& node_uid,
                                                      std::string& add_key_buffer,
                                                      const std::vector<designer::PropertyGridSection>& grid) {
    std::optional<PropertyEdit> committed;

    bool key_taken = false;
    for (const designer::PropertyGridSection& section : grid) {
        for (const designer::PropertyGridRow& row : section.rows) {
            if (row.metadata.name == add_key_buffer && row.source != designer::PropertySource::Default) {
                key_taken = true;
            }
        }
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
    ImGui::InputTextWithHint("##add_custom_key", util::Tr("properties.new_key_hint").c_str(), &add_key_buffer);
    ImGui::SameLine();
    const bool can_add = !add_key_buffer.empty() && !key_taken;
    ImGui::BeginDisabled(!can_add);
    if (ImGui::SmallButton("+")) {
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kAddProperty, add_key_buffer, ""};
        add_key_buffer.clear();
    }
    ImGui::EndDisabled();

    return committed;
}

std::string TrFormat(const std::string& key, const std::string& arg) {
    std::string result = util::Tr(key);
    const size_t pos = result.find("%s");
    if (pos == std::string::npos) return result;
    return result.substr(0, pos) + arg + result.substr(pos + 2);
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
    if (state.editable && !state.grid.empty()) {
        for (designer::PropertyGridSection& section : state.grid) {
            if (section.category == designer::PropertyCategory::Identity) continue;
            if (auto edit = DrawPropertyGridSection(section, state.source_tab_id, state.selected_node_id)) {
                committed = edit;
            }
        }
        ImGui::Spacing();
        if (auto edit = DrawAddCustomPropertyRow(state.source_tab_id, state.selected_node_id, add_property_key,
                                                  state.grid)) {
            committed = edit;
        }
    } else if (state.editable) {
        if (auto edit = DrawEditableRowTable("props", state.properties, add_property_key,
                                              state.source_tab_id, state.selected_node_id,
                                              PropertyEditKind::kValue, PropertyEditKind::kAddProperty,
                                              PropertyEditKind::kRemoveProperty)) {
            committed = edit;
        }

        if (auto edit = DrawEditableRowTable("events", state.events, add_event_key, state.source_tab_id,
                                              state.selected_node_id, PropertyEditKind::kEvent,
                                              PropertyEditKind::kEvent, PropertyEditKind::kRemoveEvent)) {
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

    if (!state.editable && !state.events.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted(util::Tr("properties.section_events").c_str());
        if (ImGui::BeginTable("events", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
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
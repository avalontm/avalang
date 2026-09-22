#include "panels/properties_panel.h"

#include <algorithm>
#include <cctype>
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
    if (row.metadata.hasRange) {
        const std::string format = row.metadata.unit.empty() ? "%.2f" : ("%.2f " + row.metadata.unit);
        ImGui::SliderScalar("##value", ImGuiDataType_Double, &number, &row.metadata.minValue,
                             &row.metadata.maxValue, format.c_str());
    } else {
        ImGui::InputDouble("##value", &number, 0.0, 0.0, "%.6g");
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (row.metadata.hasRange) {
            number = std::max(row.metadata.minValue, std::min(row.metadata.maxValue, number));
        }
        std::ostringstream out;
        out << number;
        row.value = out.str();
        committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name, row.value};
    }
    return committed;
}

std::optional<PropertyEdit> DrawEnumRow(designer::PropertyGridRow& row, int tab_id, const std::string& node_uid) {
    std::optional<PropertyEdit> committed;

    // Camino declarativo (Fase 1): el control ya listó sus opciones explícitas.
    // Fallback: extraer alternativas de un regex en `validation`, para
    // controles que aún no se migraron a metadata explícita.
    std::vector<std::pair<std::string, std::string>> options = row.metadata.enumOptions;
    if (options.empty()) {
        for (const std::string& value : ExtractRegexAlternatives(row.metadata.validation)) {
            options.emplace_back(value, value);
        }
    }

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

    std::string currentLabel = row.value;
    for (const auto& option : options) {
        if (option.first == row.value) {
            currentLabel = option.second;
            break;
        }
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", currentLabel.c_str())) {
        for (const auto& option : options) {
            const bool is_selected = (option.first == row.value);
            if (ImGui::Selectable(option.second.c_str(), is_selected)) {
                if (option.first != row.value) {
                    row.value = option.first;
                    committed = PropertyEdit{tab_id, node_uid, PropertyEditKind::kValue, row.metadata.name,
                                              option.first};
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

std::string TrFormat(const std::string& key, const std::string& arg) {
    std::string result = util::Tr(key);
    const size_t pos = result.find("%s");
    if (pos == std::string::npos) return result;
    return result.substr(0, pos) + arg + result.substr(pos + 2);
}

struct AddPropertyState {
    std::string buffer;
    std::string node_uid;
    int highlighted = 0;
};

struct AddSuggestion {
    const designer::AddablePropertyOption* option = nullptr;
    std::string key;
};

struct SuggestionNavigation {
    int highlighted = 0;
    int count = 0;
};

constexpr int kMaxVisibleSuggestions = 8;

int NavigateSuggestions(ImGuiInputTextCallbackData* data) {
    SuggestionNavigation* navigation = static_cast<SuggestionNavigation*>(data->UserData);
    if (navigation->count <= 0) return 0;
    if (data->EventKey == ImGuiKey_UpArrow) {
        navigation->highlighted = (navigation->highlighted + navigation->count - 1) % navigation->count;
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        navigation->highlighted = (navigation->highlighted + 1) % navigation->count;
    }
    return 0;
}

std::string LowerAscii(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

std::string Trim(const std::string& text) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto first = std::find_if_not(text.begin(), text.end(), is_space);
    const auto last = std::find_if_not(text.rbegin(), text.rend(), is_space).base();
    return first < last ? std::string(first, last) : std::string();
}

bool IsValidCustomKey(const std::string& key) {
    return !key.empty() &&
           std::none_of(key.begin(), key.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

bool IsKeyInGrid(const std::vector<designer::PropertyGridSection>& grid, const std::string& key) {
    for (const designer::PropertyGridSection& section : grid) {
        for (const designer::PropertyGridRow& row : section.rows) {
            if (row.metadata.name == key) return true;
        }
    }
    return false;
}

const designer::AddablePropertyOption* FindOption(const std::vector<designer::AddablePropertyOption>& addable,
                                                   const std::string& key) {
    for (const designer::AddablePropertyOption& option : addable) {
        if (option.metadata.name == key) return &option;
    }
    return nullptr;
}

std::vector<AddSuggestion> BuildSuggestions(const std::vector<designer::AddablePropertyOption>& addable,
                                             const std::string& key, bool custom_allowed) {
    const std::string needle = LowerAscii(key);
    std::vector<AddSuggestion> prefix_matches;
    std::vector<AddSuggestion> partial_matches;
    for (const designer::AddablePropertyOption& option : addable) {
        const std::string name = LowerAscii(option.metadata.name);
        if (needle.empty() || name.rfind(needle, 0) == 0) {
            prefix_matches.push_back(AddSuggestion{&option, option.metadata.name});
        } else if (name.find(needle) != std::string::npos) {
            partial_matches.push_back(AddSuggestion{&option, option.metadata.name});
        }
    }

    std::vector<AddSuggestion> suggestions = std::move(prefix_matches);
    suggestions.insert(suggestions.end(), partial_matches.begin(), partial_matches.end());
    if (custom_allowed && FindOption(addable, key) == nullptr) {
        suggestions.push_back(AddSuggestion{nullptr, key});
    }
    return suggestions;
}

PropertyEdit MakeAddEdit(int tab_id, const std::string& node_uid, const AddSuggestion& suggestion) {
    if (suggestion.option) {
        const PropertyEditKind kind =
            suggestion.option->declared ? PropertyEditKind::kValue : PropertyEditKind::kAddProperty;
        return PropertyEdit{tab_id, node_uid, kind, suggestion.option->metadata.name,
                            suggestion.option->initialValue};
    }
    return PropertyEdit{tab_id, node_uid, PropertyEditKind::kAddProperty, suggestion.key, ""};
}

ImVec2 SuggestionPopupPlacement(const ImVec2& input_min, const ImVec2& input_max, float popup_height,
                                ImVec2* pivot) {
    const ImGuiViewport* viewport = ImGui::GetWindowViewport();
    const float space_below = viewport->WorkPos.y + viewport->WorkSize.y - input_max.y;
    const float space_above = input_min.y - viewport->WorkPos.y;
    if (space_below < popup_height && space_above > space_below) {
        *pivot = ImVec2(0.0f, 1.0f);
        return ImVec2(input_min.x, input_min.y);
    }
    *pivot = ImVec2(0.0f, 0.0f);
    return ImVec2(input_min.x, input_max.y);
}

std::optional<PropertyEdit> DrawSuggestionPopup(const char* popup_id, int tab_id, const std::string& node_uid,
                                                 AddPropertyState& state,
                                                 const std::vector<AddSuggestion>& suggestions,
                                                 const ImVec2& input_min, const ImVec2& input_max,
                                                 bool navigation_moved, bool close_requested) {
    std::optional<PropertyEdit> committed;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float max_height = line_height * kMaxVisibleSuggestions + style.WindowPadding.y * 2.0f;
    const float rows = static_cast<float>(std::max<size_t>(suggestions.size(), 1));
    const float estimated_height = std::min(max_height, line_height * rows + style.WindowPadding.y * 2.0f);
    const float width = std::max(input_max.x - input_min.x, 160.0f);

    ImVec2 pivot;
    const ImVec2 position = SuggestionPopupPlacement(input_min, input_max, estimated_height, &pivot);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, max_height));

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (!ImGui::BeginPopup(popup_id, flags)) {
        return committed;
    }

    if (close_requested || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return committed;
    }

    if (suggestions.empty()) {
        ImGui::TextDisabled("%s", util::Tr("properties.add_no_results").c_str());
    }

    const ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < static_cast<int>(suggestions.size()); ++i) {
        const AddSuggestion& suggestion = suggestions[i];
        const bool is_highlighted = (i == state.highlighted);
        const std::string label =
            suggestion.option ? suggestion.option->metadata.name
                              : TrFormat("properties.add_custom_entry", suggestion.key);

        ImGui::PushID(i);
        const float row_start = ImGui::GetCursorPosX();
        const float row_width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable(label.c_str(), is_highlighted)) {
            committed = MakeAddEdit(tab_id, node_uid, suggestion);
        }
        if (ImGui::IsItemHovered()) {
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) state.highlighted = i;
            if (suggestion.option && !suggestion.option->metadata.description.empty()) {
                ImGui::SetTooltip("%s", suggestion.option->metadata.description.c_str());
            }
        }
        if (is_highlighted && navigation_moved) {
            ImGui::SetScrollHereY();
        }
        if (suggestion.option) {
            const std::string tag = util::Tr(CategoryTrKey(suggestion.option->metadata.category));
            const float tag_width = ImGui::CalcTextSize(tag.c_str()).x;
            ImGui::SameLine(row_start + row_width - tag_width, 0.0f);
            ImGui::TextDisabled("%s", tag.c_str());
        }
        ImGui::PopID();

        if (committed) {
            ImGui::CloseCurrentPopup();
            break;
        }
    }

    ImGui::EndPopup();
    return committed;
}

std::optional<PropertyEdit> DrawAddPropertyRow(int tab_id, const std::string& node_uid, AddPropertyState& state,
                                                const std::vector<designer::PropertyGridSection>& grid,
                                                const std::vector<designer::AddablePropertyOption>& addable) {
    constexpr const char* kPopupId = "##add_property_suggestions";
    std::optional<PropertyEdit> committed;

    if (state.node_uid != node_uid) {
        state.node_uid = node_uid;
        state.buffer.clear();
        state.highlighted = 0;
    }

    const std::string key = Trim(state.buffer);
    const bool custom_allowed = IsValidCustomKey(key) && !IsKeyInGrid(grid, key);
    const std::vector<AddSuggestion> suggestions = BuildSuggestions(addable, key, custom_allowed);
    const int count = static_cast<int>(suggestions.size());
    state.highlighted = count == 0 ? 0 : std::clamp(state.highlighted, 0, count - 1);

    SuggestionNavigation navigation{state.highlighted, count};
    const std::string buffer_before = state.buffer;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
    const bool submitted = ImGui::InputTextWithHint(
        "##add_property_key", util::Tr("properties.new_key_hint").c_str(), &state.buffer,
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, NavigateSuggestions,
        &navigation);
    const ImVec2 input_min = ImGui::GetItemRectMin();
    const ImVec2 input_max = ImGui::GetItemRectMax();
    const bool edited = state.buffer != buffer_before;

    const bool wants_popup = ImGui::IsItemActivated() || ImGui::IsItemClicked() || edited;
    if (wants_popup && !ImGui::IsPopupOpen(kPopupId)) {
        ImGui::OpenPopup(kPopupId);
    }

    const bool navigation_moved = navigation.highlighted != state.highlighted;
    state.highlighted = edited ? 0 : navigation.highlighted;

    ImGui::SameLine();
    const bool can_add = custom_allowed || FindOption(addable, key) != nullptr;
    ImGui::BeginDisabled(!can_add);
    const bool add_clicked = ImGui::SmallButton("+");
    ImGui::EndDisabled();

    if (submitted && count > 0) {
        committed = MakeAddEdit(tab_id, node_uid, suggestions[state.highlighted]);
    } else if (add_clicked && can_add) {
        if (const designer::AddablePropertyOption* option = FindOption(addable, key)) {
            committed = MakeAddEdit(tab_id, node_uid, AddSuggestion{option, key});
        } else {
            committed = MakeAddEdit(tab_id, node_uid, AddSuggestion{nullptr, key});
        }
    }

    if (auto popup_edit = DrawSuggestionPopup(kPopupId, tab_id, node_uid, state, suggestions, input_min,
                                              input_max, navigation_moved, committed.has_value())) {
        committed = popup_edit;
    }

    if (committed) {
        state.buffer.clear();
        state.highlighted = 0;
    }

    return committed;
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
    static AddPropertyState add_property_state;

    const std::string title = util::Tr("panel.properties.title") + "###properties";
    ImGui::Begin(title.c_str(), p_open);

    const bool multi_selection = state.selected_node_ids.size() > 1;

    if (state.selected_component_type.empty() && !multi_selection) {
        ImGui::TextDisabled("%s", util::Tr("properties.empty_selection").c_str());
        ImGui::End();
        return committed;
    }

    if (multi_selection) {
        ImGui::Text("%s", TrFormat("properties.multi_selection_count",
                                    std::to_string(state.selected_node_ids.size())).c_str());
        if (state.mixed_types) {
            ImGui::TextDisabled("%s", util::Tr("properties.multi_selection_mixed_types").c_str());
            ImGui::End();
            return committed;
        }
        ImGui::TextDisabled(
            "%s", TrFormat("properties.multi_selection_type", state.selected_component_type).c_str());
    } else if (state.editable) {

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
        if (auto edit = DrawAddPropertyRow(state.source_tab_id, state.selected_node_id, add_property_state,
                                            state.grid, state.addable)) {
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

    if (!state.responsive_rows.empty()) {
        ImGui::Spacing();
        const std::string header =
            TrFormat("properties.section_responsive", std::to_string(state.responsive_viewport_width)) +
            "###responsive_section";
        if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("responsive_style", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn(util::Tr("properties.column_property").c_str());
                ImGui::TableSetupColumn(util::Tr("properties.column_value").c_str());
                ImGui::TableSetupColumn(util::Tr("properties.column_breakpoint").c_str());
                ImGui::TableHeadersRow();
                for (const designer::ResponsiveStyleRow& row : state.responsive_rows) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(row.key.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(row.value.c_str());
                    ImGui::TableSetColumnIndex(2);
                    if (row.breakpointMinWidth) {
                        ImGui::Text(">= %upx", static_cast<unsigned>(*row.breakpointMinWidth));
                    } else {
                        ImGui::TextDisabled("%s", util::Tr("properties.responsive_base").c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (committed && multi_selection) {
        switch (committed->kind) {
            case PropertyEditKind::kValue:
            case PropertyEditKind::kEvent:
            case PropertyEditKind::kRemoveProperty:
            case PropertyEditKind::kRemoveEvent:
                for (const std::string& node_id : state.selected_node_ids) {
                    if (node_id != committed->node_id) {
                        committed->extra_node_ids.push_back(node_id);
                    }
                }
                break;
            default:
                break;
        }
    }

    ImGui::End();
    return committed;
}

}
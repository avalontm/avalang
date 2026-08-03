#include "panels/properties_panel.h"

#include "design/component_catalog.h"
#include "imgui.h"
#include "imgui_stdlib.h" // ImGui::InputText(const char*, std::string*) overload

namespace studio {

namespace {

// Small "x" remove button, right-aligned in whatever column it's
// placed in -- shared by the properties and events tables below so
// removing a row looks/behaves identically in both. Returns true the
// one frame it's clicked.
bool DrawRemoveButton(const char* str_id) {
    ImGui::PushID(str_id);
    const bool clicked = ImGui::SmallButton("x");
    ImGui::PopID();
    return clicked;
}

// One editable key/value table (properties or events -- same shape),
// with a per-row remove button and an "add new row" line underneath.
// `add_buffer` is the caller's own persistent std::string for the
// "new key" input -- kept outside this function (in DrawPropertiesPanel's
// local statics) so it survives across frames while the person is
// typing a new key, same reasoning as row.value being bound directly
// instead of copied into a temp buffer.
//
// Returns a PropertyEdit for whichever single row-level action (value
// committed, row removed, row added) happened this frame, using
// `value_kind`/`add_kind`/`remove_kind` to tag it correctly for
// properties vs. events -- the table drawing/interaction logic itself
// doesn't otherwise care which one it's rendering.
std::optional<PropertyEdit> DrawEditableRowTable(const char* table_id, std::vector<PropertyRow>& rows,
                                                  std::string& add_key_buffer, int tab_id,
                                                  const std::string& node_uid, PropertyEditKind value_kind,
                                                  PropertyEditKind add_kind, PropertyEditKind remove_kind) {
    std::optional<PropertyEdit> committed;

    if (ImGui::BeginTable(table_id, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        // Index-based removal deferred until after the loop (erasing
        // mid-iteration would invalidate `rows`' own iterators/indices
        // for whatever's left of this same frame's loop).
        int remove_index = -1;

        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            PropertyRow& row = rows[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.key.c_str());
            ImGui::TableSetColumnIndex(1);

            // PushID by index (not by key -- keys aren't guaranteed
            // unique within a node's property/event list) so every row
            // gets its own stable ImGui widget identity for the frames
            // it exists across.
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(-FLT_MIN);
            // Bound directly to row.value -- this is what makes the
            // table itself reflect keystrokes immediately, no extra
            // "editing buffer" struct to keep in sync. What's NOT
            // committed to the real DesignNode until deactivation
            // below is only the *source-of-truth* copy living in some
            // EditorTab's DesignDocument -- see PropertyEdit's comment
            // in properties_panel.h.
            ImGui::InputText("##value", &row.value);
            // Deactivated-after-edit (unfocus or Enter), not every
            // keystroke -- matches how every other text-entry point in
            // this codebase commits, and avoids re-parsing/re-dirtying
            // the document on every single character typed.
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
            // Mirror the removal locally too, so the table doesn't
            // still show the just-deleted row for one extra frame
            // while main.cpp's write-back (which patches the *real*
            // DesignNode, not this PropertiesState's own copy) catches
            // up on its own next call -- same "why mirror this here"
            // reasoning as tab.design.dirty/tab.dirty in main.cpp.
            rows.erase(rows.begin() + remove_index);
        }

        // "Add new row" line -- own row, not inside the loop above, so
        // it never gets an index that collides with a real row's ID.
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::PushID("##add_key");
        ImGui::InputTextWithHint("##add_key", "nueva key", &add_key_buffer);
        ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("--");
        ImGui::TableSetColumnIndex(2);
        // Disabled with an empty key (nothing to add) or a key that
        // already exists (silently skipping a duplicate is friendlier
        // than adding a second, indistinguishable row with the same
        // key -- the table has no way to tell them apart afterward).
        bool key_taken = false;
        for (const PropertyRow& row : rows) {
            if (row.key == add_key_buffer) { key_taken = true; break; }
        }
        const bool can_add = !add_key_buffer.empty() && !key_taken;
        ImGui::BeginDisabled(!can_add);
        if (ImGui::SmallButton("+")) {
            committed = PropertyEdit{tab_id, node_uid, add_kind, add_key_buffer, ""};
            rows.push_back(PropertyRow{add_key_buffer, ""}); // same local-mirror reasoning as removal above
            add_key_buffer.clear();
        }
        ImGui::EndDisabled();

        ImGui::EndTable();
    }

    return committed;
}

} // namespace

std::optional<PropertyEdit> DrawPropertiesPanel(PropertiesState& state, bool* p_open) {
    std::optional<PropertyEdit> committed;

    // Persist the two "add new key" text buffers across frames --
    // static is fine here (single Properties panel, one selection at a
    // time) rather than threading them through PropertiesState itself,
    // which every OTHER caller (Preview's read-only path) would then
    // have to carry around for no reason. Whatever's mid-typed here is
    // harmless leftover text if the selection changes mid-edit -- worst
    // case an add click after switching selection adds to the newly
    // selected node instead, same "no destructive default" spirit as
    // the rest of this fase.
    static std::string add_property_key;
    static std::string add_event_key;

    ImGui::Begin("Properties", p_open);

    if (state.selected_component_type.empty()) {
        ImGui::TextDisabled("Seleccioná un componente en el canvas para ver y editar sus propiedades.");
        ImGui::End();
        return committed;
    }

    if (state.editable) {
        // Type as an editable combo, seeded from the same catalog the
        // Toolbox drags from -- picking a different type only changes
        // DesignNode::type; it deliberately does NOT re-seed/merge
        // default_properties for the new type (that could silently
        // discard hand-edited values), so existing properties/events
        // just carry over untouched even if some no longer apply to
        // the new type. Simple and predictable beats clever here.
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::TextUnformatted("Type");
        if (ImGui::BeginCombo("##type_combo", state.selected_component_type.c_str())) {
            for (const design::ComponentTypeInfo& info : design::GetComponentCatalog()) {
                const bool is_selected = (info.type == state.selected_component_type);
                if (ImGui::Selectable(info.display_name.c_str(), is_selected)) {
                    if (info.type != state.selected_component_type) {
                        committed = PropertyEdit{state.source_tab_id, state.selected_node_uid,
                                                  PropertyEditKind::kType, "", info.type};
                        state.selected_component_type = info.type; // local mirror, see removal comment above
                    }
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TextUnformatted("Id");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##id_value", &state.selected_component_id);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            committed = PropertyEdit{state.source_tab_id, state.selected_node_uid, PropertyEditKind::kId, "",
                                      state.selected_component_id};
        }
    } else {
        ImGui::Text("Type: %s", state.selected_component_type.c_str());
        if (!state.selected_component_id.empty()) {
            ImGui::Text("Id: %s", state.selected_component_id.c_str());
        }
        // Either a Preview-panel selection (no source file to write
        // into at all) or a synthetic Designer selection (a resolved
        // `Componente()` import copy, see designer_canvas.h): either
        // way there's nowhere real to write an edit back into, so the
        // tables below stay read-only and this says so.
        ImGui::TextDisabled("Read-only (no editable source for this selection).");
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Properties");
    if (state.editable) {
        if (auto edit = DrawEditableRowTable("props", state.properties, add_property_key,
                                              state.source_tab_id, state.selected_node_uid,
                                              PropertyEditKind::kValue, PropertyEditKind::kAddProperty,
                                              PropertyEditKind::kRemoveProperty)) {
            committed = edit;
        }
    } else if (ImGui::BeginTable("props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");
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

    // Events -- shown for a Designer selection only (Preview's demo
    // tree never populated PropertiesState::events, see the header
    // comment; an empty read-only table here would just be noise for
    // it). Same editable/read-only split as Properties above.
    if (state.editable || !state.events.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Events");
        if (state.editable) {
            if (auto edit = DrawEditableRowTable("events", state.events, add_event_key, state.source_tab_id,
                                                  state.selected_node_uid, PropertyEditKind::kEvent,
                                                  PropertyEditKind::kEvent, PropertyEditKind::kRemoveEvent)) {
                committed = edit;
            }
        } else if (ImGui::BeginTable("events", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Event");
            ImGui::TableSetupColumn("Handler");
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

} // namespace studio

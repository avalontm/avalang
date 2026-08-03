#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace studio {

struct PropertyRow {
    std::string key;
    std::string value; // display-ready string, already stringified
};

struct PropertiesState {
    std::string selected_component_type; // e.g. "button" -- empty = nothing selected
    std::string selected_component_id;
    std::vector<PropertyRow> properties;

    // DesignNode::events mirror (key = event name e.g. "on_click",
    // value = handler function name) -- only populated for a Designer
    // canvas selection (see designer_canvas.cpp's ToPropertiesState).
    // Preview-panel selections leave this empty, same as `properties`
    // conceptually always existed there but events never did.
    std::vector<PropertyRow> events;

    // Below: only meaningful when a Designer canvas selection populated
    // this state (see designer_canvas.cpp's ToPropertiesState) -- the
    // Preview panel's read-only demo tree (preview_panel.cpp) leaves
    // these at their defaults, which keeps `editable` false there and
    // the table read-only, same as before this fase.

    // False for every Preview-panel selection, and also false for a
    // Designer canvas selection that landed on a *synthetic* node (a
    // resolved `Componente()` import copy, see designer_canvas.h) --
    // there's no real DesignNode in any doc.root to write back into for
    // those. True only for a selection on a real node of the active
    // .avaui's own tree.
    bool editable = false;

    // Identifies which node to patch on write-back: `source_tab_id`
    // matches EditorTab::id (stable across tab reordering/renaming,
    // unlike a vector index) and `selected_node_uid` matches
    // DesignNode::node_uid within that tab's DesignDocument::root.
    // Both stay at their defaults (-1 / empty) for a non-editable
    // selection.
    int source_tab_id = -1;
    std::string selected_node_uid;
};

// What a PropertyEdit represents -- previously (Fase 3) there was only
// ever one kind of edit (a property's value), so PropertyEdit didn't
// need to say which; now (9.9/9.12's leftover items -- id, type,
// add/remove properties, events) there are several, so every
// PropertyEdit says which one it is and main.cpp's write-back switches
// on it.
enum class PropertyEditKind {
    kValue,          // an existing property's value changed -- key + new_value
    kId,             // DesignNode::id changed -- new_value holds the new id, key unused
    kType,           // DesignNode::type changed -- new_value holds the new type string, key unused
    kAddProperty,    // a new property row -- key = its key, new_value = its (usually empty) initial value
    kRemoveProperty, // remove the property with this key -- key set, new_value unused
    kEvent,          // an event's handler changed or a new event row was added -- key = event name, new_value = handler
    kRemoveEvent,    // remove the event with this key -- key set, new_value unused
};

// One committed property edit, returned by DrawPropertiesPanel when the
// person finishes editing a value (see ImGui::IsItemDeactivatedAfterEdit
// in properties_panel.cpp -- committed on unfocus/Enter, not keystroke by
// keystroke) or clicks an add/remove button. The caller (main.cpp) is the
// one that actually knows where every open DesignDocument lives, so it
// looks up `source_tab_id` / `node_uid` itself and patches the real
// DesignNode according to `kind` -- this struct is just the "what
// changed" message, not a mutation applied by this panel.
struct PropertyEdit {
    int tab_id = -1;
    std::string node_uid;
    PropertyEditKind kind = PropertyEditKind::kValue;
    std::string key;
    std::string new_value;
};

// Draws the Properties panel (right dock). Read-only unless
// `state.editable` is set (only true for a real, non-synthetic Designer
// canvas selection -- see PropertiesState::editable above): each row's
// value becomes an editable text field, bound directly to
// `state.properties[i].value` (via imgui_stdlib's std::string overload)
// so the table itself always shows what's being typed, plus (when
// editable) an editable Id field, a Type combo seeded from
// design::GetComponentCatalog(), a remove button per property/event
// row, and an "add" row at the bottom of each table. Returns a
// PropertyEdit once any of that is committed (nullopt on every frame
// nothing was just committed) so the caller can write it back into the
// real DesignNode and mark the document dirty.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
std::optional<PropertyEdit> DrawPropertiesPanel(PropertiesState& state, bool* p_open = nullptr);

} // namespace studio

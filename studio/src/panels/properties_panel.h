#pragma once

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
};

// Draws the Properties panel (right dock). Milestone 1: read-only,
// reflects whatever the Preview panel currently has selected. Editing
// (which writes back into the source .ava) is the next milestone --
// see PROPERTIES_EDITABLE note in preview_panel.cpp.
void DrawPropertiesPanel(const PropertiesState& state);

} // namespace studio

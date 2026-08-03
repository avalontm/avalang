#pragma once

#include <optional>

#include "engine/engine_bridge.h"
#include "panels/properties_panel.h"

namespace studio {

// Draws the Preview panel (bottom dock, per the Explorer/Designer/
// Properties/Preview sketch). Milestone 1: renders the fixed demo
// Component Tree as a clickable outline (not the real drag/drop
// Designer canvas yet -- that's the next milestone, once
// `page`/`stack`/`button` builtins let a real .ava script produce this
// tree instead of BuildDemoComponentTree()).
//
// Clicking a node returns its properties so the caller can feed the
// Properties panel -- see PROPERTIES_EDITABLE below for what's still
// read-only.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
std::optional<PropertiesState> DrawPreviewPanel(const EngineBridge::PreviewNode& root, bool* p_open = nullptr);

} // namespace studio

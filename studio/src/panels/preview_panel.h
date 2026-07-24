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
std::optional<PropertiesState> DrawPreviewPanel(const EngineBridge::PreviewNode& root);

} // namespace studio

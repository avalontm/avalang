#pragma once

#include <string>

#include "engine/engine_bridge.h"

namespace studio {

struct OutputState {
    bool has_run_result = false;
    RunResult last_run;
    std::string last_tree_json;
};

// Draws the Output/Console panel (bottom dock). Shows the result of the
// last Run (F5) and, once available, the JSON dump proving the
// Component Tree round-trip through ava_ui_tree_to_json.
void DrawOutputPanel(const OutputState& state);

} // namespace studio

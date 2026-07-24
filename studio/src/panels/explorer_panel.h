#pragma once

#include <optional>
#include <string>

namespace studio {

struct ExplorerState {
    std::string root_dir; // folder being browsed, e.g. the project's scripts/ dir
};

// Draws the Explorer panel (left dock). Returns the path of a file the
// user clicked, if any -- caller feeds that into the Editor panel.
std::optional<std::string> DrawExplorerPanel(ExplorerState& state);

} // namespace studio

#pragma once

#include <optional>
#include <string>

namespace studio {

struct ExplorerState {
    std::string root_dir; // folder being browsed, e.g. the project's scripts/ dir
};

struct ExplorerResult {
    // File the user double-clicked -- caller opens it in the Editor panel.
    // Single-click only selects the row (VSCode-style: a single click on
    // a file in the tree doesn't steal focus into a new/existing tab).
    std::optional<std::string> file_to_open;
    // File the user deleted via the right-click "Delete" confirmation --
    // caller should close its tab if it happens to be open, since the
    // file backing it no longer exists on disk.
    std::optional<std::string> file_deleted;
};

// Draws the Explorer panel (left dock).
ExplorerResult DrawExplorerPanel(ExplorerState& state);

} // namespace studio

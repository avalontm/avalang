#pragma once

#include <optional>
#include <string>
#include <utility>

namespace studio {

struct ExplorerState {
    std::string root_dir; // folder being browsed, e.g. the project's scripts/ dir

    // Currently selected row (single click, or right-click for the
    // context menu) -- empty means nothing selected. This is what F2/Del
    // act on, and what the row-highlight in the tree reflects. Can be
    // either a file or a folder path.
    std::string selected_path;
};

struct ExplorerResult {
    // File the user double-clicked -- caller opens it in the Editor panel.
    // Single-click only selects the row (VSCode-style: a single click on
    // a file in the tree doesn't steal focus into a new/existing tab).
    std::optional<std::string> file_to_open;
    // Path the user deleted via the right-click "Delete" confirmation (or
    // the Del hotkey) -- a file or a whole folder (with its contents).
    // Caller should close any open tab(s) under this path, since the
    // file(s) backing them no longer exist on disk.
    std::optional<std::string> file_deleted;
    // {old_path, new_path} when the user renamed a file/folder via the
    // right-click "Rename" menu item (or the F2 hotkey), or moved one by
    // dragging it onto another folder (or onto empty space, for the
    // project root) in the tree. Caller should retarget any open tab(s)
    // (EditorTab::file_path) under old_path so they keep tracking the
    // same file instead of the stale path.
    std::optional<std::pair<std::string, std::string>> file_renamed;
};

// Draws the Explorer panel (left dock).
ExplorerResult DrawExplorerPanel(ExplorerState& state);

} // namespace studio

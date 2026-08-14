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
    // Path the user picked via the right-click "Open in Explorer" menu
    // item -- a file or a folder. Caller (main.cpp, which owns the
    // platform-specific window/shell code) reveals it in the OS file
    // manager via studio::titlebar::RevealInFileExplorer.
    std::optional<std::string> reveal_in_file_manager;
};

// Draws the Explorer panel (left dock). `p_open`: same convention as
// ImGui::Begin's own p_open -- pass the address of this panel's runtime
// visibility flag (see main.cpp's `panel_open` map) so the tab gets a
// close ("x") button and clicking it flips the flag to false, the same
// way the View menu's checkbox does. nullptr (the default) draws the
// panel with no close button, same as before this parameter existed.
ExplorerResult DrawExplorerPanel(ExplorerState& state, bool* p_open = nullptr);

} // namespace studio

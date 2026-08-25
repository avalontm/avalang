#pragma once

#include <optional>
#include <string>
#include <vector>

namespace studio {

struct QuickOpenState {
    struct Entry {
        std::string display;    // relative to project_root, shown in the list
        std::string full_path;  // same path convention ExplorerState/OpenFileInTab
                                 // already use -- returned on pick, not `display`
    };

    std::string query;
    int selected_index = 0;

    // Rebuilt each time the palette opens (see OpenQuickOpen), not kept
    // fresh continuously -- same "no watcher/debounce infrastructure
    // exists yet" reasoning §3.3 already gives for Check not running
    // automatically on save. A stale list only matters between filesystem
    // changes and the next Ctrl+P, which this rebuild already covers.
    std::vector<Entry> files;

    // Mirrors CommandPaletteState::focus_query_field -- set by
    // OpenQuickOpen, consumed (and cleared) by DrawQuickOpen on the next
    // frame it draws.
    bool focus_query_field = false;
};

// Rescans project_root for ".ava"/".avaui" files (via
// ListSearchableFiles in project_utils.cpp, the same helper Find in
// Project uses), resets query/selected_index, arms focus_query_field, and
// calls ImGui::OpenPopup() exactly once -- same idiom as
// OpenCommandPalette. Callers must invoke this from an edge-triggered
// condition (a want_* boolean), not every frame.
void OpenQuickOpen(QuickOpenState& state, const std::string& project_root);

// Must be called every frame regardless of whether Quick Open is open --
// same requirement DrawCommandPalette has for its own popup. No-ops
// (returns nullopt immediately after a failed BeginPopupModal) when the
// palette isn't open.
//
// Filtering is a plain case-insensitive substring match against the
// displayed relative path (same scope as Find in Project/Command
// Palette -- no fuzzy scoring). Up/Down move the selection, Enter picks
// the selected file and closes the popup, Escape closes without picking
// anything. Returns the picked file's full_path on the frame it's picked
// (by Enter or by clicking a row); the caller is expected to call
// OpenFileInTab with it, same two-step pattern already used for
// Terminal/Problems/Find in Project file clicks in main.cpp.
std::optional<std::string> DrawQuickOpen(QuickOpenState& state);

}

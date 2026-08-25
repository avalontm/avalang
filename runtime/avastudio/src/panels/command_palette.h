#pragma once

#include <functional>
#include <string>
#include <vector>

namespace studio {

// One entry in the palette. `label` is already fully built/translated by the
// caller as "Category: Action" (main.cpp assembles it via util::Tr() on a
// category key + an action key, same "Category: Action" convention VSCode
// uses) -- the palette itself doesn't know about i18n, it just filters and
// displays strings, same division of labor as FindInProjectPanel not
// knowing what a "project" means beyond a root path. `shortcut` is display
// only (shown right-aligned, like the menu items in titlebar_panel.cpp);
// it does not register or intercept the key itself -- the real shortcut
// still lives wherever it already did (main.cpp's want_* booleans), so a
// command and its menu/shortcut equivalent can never disagree about what
// they do, only about how they're triggered.
struct Command {
    std::string id;
    std::string label;
    std::string shortcut;
    std::function<void()> action;
};

struct CommandPaletteState {
    std::string query;
    int selected_index = 0;

    // Mirrors FindInProjectState::focus_query_field -- set by OpenCommandPalette,
    // consumed (and cleared) by DrawCommandPalette on the next frame it draws.
    bool focus_query_field = false;
};

// Resets state.query/selected_index, arms focus_query_field, and calls
// ImGui::OpenPopup() exactly once -- same idiom already used for the
// "Unsaved Changes" modal in main.cpp: OpenPopup only fires on the frame the
// shortcut/menu item is pressed, never every frame, so callers must call
// this from an edge-triggered condition (a want_* boolean), not every frame.
void OpenCommandPalette(CommandPaletteState& state);

// Must be called every frame regardless of whether the palette is open --
// same requirement ImGui::BeginPopupModal has for any modal, matching how
// the "Unsaved Changes" confirm in main.cpp is already unconditionally
// reached each frame. No-ops (returns immediately after a failed
// BeginPopupModal) when the palette isn't open.
//
// Filtering is a plain case-insensitive substring match against `label`
// (same scope as Find in Project's search -- no fuzzy scoring). Up/Down
// move the selection, Enter runs the selected command's action and closes
// the popup, Escape closes without running anything.
void DrawCommandPalette(CommandPaletteState& state, const std::vector<Command>& commands);

}

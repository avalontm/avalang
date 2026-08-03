#pragma once

#include <optional>
#include <string>

#include "engine/engine_bridge.h"

namespace studio {

// UI-only state for the Terminal panel. The actual scrollback (the
// ConsoleLine history) lives in EngineBridge, not here -- see the
// comment on EngineBridge::Console() for why (it's tied to the VM's
// print callback, which outlives any one panel draw call).
struct TerminalState {
    std::string input_buffer;    // scratch buffer for the console's input box widget
    bool has_run_result = false; // true once the user has pressed Run at least once this session
    RunResult last_run;          // last run's summary -- lets other UI (e.g. a future status bar) show success/failure without re-scanning the console

    // Selection range over console line indices, both inclusive. -1 means
    // "no selection". anchor is where the click/drag started, cursor is
    // where it currently ends (they can be in either order -- callers use
    // std::min/max of the pair to get the actual [first, last] range).
    int selection_anchor = -1;
    int selection_cursor = -1;
};

// Returned by DrawTerminalPanel when the user clicks an Error line in the
// console that carries a known source position (ConsoleLine::error_line
// != 0 -- see engine_bridge.h). main.cpp opens/focuses `file_path`'s tab
// and highlights `line`/`column` on it, the same way it already does
// right after a failed Run -- this just lets an *older* error line in
// the scrollback jump there again, without re-running anything.
struct TerminalFileClickRequest {
    std::string file_path;
    int line = 0;
    int column = 0;
    std::string message; // ConsoleLine::text, for HighlightError's hover tooltip
};

// Draws the Terminal panel (bottom dock) as an execution console: every
// print() from the running script, interleaved with Run/error/result
// markers, accumulated across every run this session like a real
// terminal -- replaces the old static "last run result + Component Tree
// JSON" view. Only ever shows a script's own output; anything else
// (plugin lifecycle messages, host events) goes to the separate Output
// panel instead -- see panels/logs_panel.h and util/log_bridge.h.
//
// The bottom input line calls EngineBridge::SubmitConsoleInput() on
// Enter and echoes the text into the console, but nothing in the
// language reads from it yet -- there is no `input()` builtin. See
// engine_bridge.h for why and what's needed before there can be one;
// this is scaffolding for that, not a working REPL today.
//
// Returns a request when an Error line with a known source position was
// clicked this frame (see TerminalFileClickRequest above); nullopt every
// other frame. Consumed by main.cpp right after the call, same pattern
// as DrawPreviewPanel's return value.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
std::optional<TerminalFileClickRequest> DrawTerminalPanel(TerminalState& state, EngineBridge& engine,
                                                            bool* p_open = nullptr);

} // namespace studio

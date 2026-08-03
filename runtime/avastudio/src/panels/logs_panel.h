#pragma once

#include "util/log_bridge.h"

namespace studio {

// UI-only state for the Logs panel. Mirrors TerminalState's selection
// fields (see panels/terminal_panel.h) but there's no input box and no
// click-to-file-position here -- general log lines don't carry a source
// position the way a script's compile errors do.
struct LogsState {
    int selection_anchor = -1;
    int selection_cursor = -1;
};

// Draws the Output panel (bottom dock) as a plain, append-only log of
// everything that isn't a script's own run output: plugin load/init
// messages, PluginHost edit-apply results, etc. -- see LogBridge's
// header comment for why this is a separate stream from the Terminal's
// console. Visually the same list/copy/clear idiom as the Terminal, just
// without the run-input box or error-click navigation.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
void DrawLogsPanel(LogsState& state, LogBridge& log_bridge, bool* p_open = nullptr);

} // namespace studio

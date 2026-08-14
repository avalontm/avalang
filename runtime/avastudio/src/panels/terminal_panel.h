#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "engine/engine_bridge.h"

namespace studio {

// Background-thread state for one in-flight "Run" (see StartScriptRun
// below). Same shape as BuildPanelState's own worker fields
// (panels/build_panel.h) -- deliberately, since it's the same pattern:
// the script runs out-of-process via ava_cli.exe instead of calling
// EngineBridge::RunScript() directly on the UI thread, so that:
//
//  - A script that blocks on a native `extern` call (e.g. libmysql's
//    mysql_real_connect with nothing listening on the other end) can no
//    longer freeze the whole window -- AvaLang's coroutines/async
//    (builtins/builtin_async.cpp) only cover bytecode-level yields and
//    timers, they cannot hand control back mid a blocking native call
//    (see vm_extern.cpp's ffi_call), so the only way to keep the UI
//    thread free is to not run the blocking call ON the UI thread.
//  - A hard native crash inside the script (e.g. an out-of-bounds
//    pointer read through mem_peek_ptr, which is intentionally unchecked
//    -- see builtins/builtin_mem.cpp) only takes down the child
//    ava_cli.exe process, not AvaStudio itself.
//
// Trade-off, and it's a real one: EngineBridge::RunScript() (still used
// for plugins' run_project_on_main_thread, see main.cpp) gets precise
// error_line/error_column/error_source straight from
// ava_last_error_line/column/source. ava_cli.exe today only prints
// "compile error: ..." / "runtime error: ..." to stderr (see
// runtime/avacli/src/main.cpp) with no structured position -- so a run
// through this path can't click-to-jump to the offending line the way
// an in-process run's Error console lines can. The error text itself
// still comes through fine, just not the line highlight.
struct ScriptRunState {
    std::atomic<bool> running{false};
    std::thread worker;

    std::mutex mutex; // guards every field below
    std::string log;  // ava_cli's stdout+stderr, interleaved, as it streams in
    std::string::size_type log_forwarded_upto = 0;
    bool has_result = false;    // true once the process has exited (or failed to launch)
    bool result_consumed = false; // DrawTerminalPanel flips this once it has folded has_result into the console/TerminalState::last_run, so it only does that once
    bool launch_failed = false; // true if ava_cli_path itself couldn't even be started (bad path) -- distinct from exit_code, which only means anything once a process actually ran
    int exit_code = -1;         // 0 = success, 1 = normal compile/runtime error (see ava_cli's main.cpp), anything else = the child process itself crashed
};

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

    // Backing state for the interactive Run button/shortcut (main.cpp),
    // polled and folded into EngineBridge::Console()/last_run above by
    // DrawTerminalPanel every frame. See ScriptRunState's own comment
    // for why Run works this way instead of calling
    // EngineBridge::RunScript() directly.
    ScriptRunState run;
};

// Starts running `script_path` via `ava_cli_path` (e.g. the result of
// DetectAvaCliPath() / StudioSettings::build_ava_cli_path -- same
// resolution the Build panel already uses, see util/ava_cli_locator.h)
// on a background thread. No-op if a run is already in flight
// (state.run.running) -- caller should disable/hide the Run action
// while that's true, same convention as BuildPanelState::building. Joins
// any previous (already-finished) worker thread first, same as
// StartBuild in build_panel.cpp.
//
// Does NOT save the active tab first -- callers must ensure
// `script_path` reflects what they want to actually run (main.cpp calls
// SaveTab() on the active tab right before this, since ava_cli.exe reads
// the file from disk, unlike the old in-process RunScript() which ran
// the editor's in-memory buffer directly).
void StartScriptRun(TerminalState& state, EngineBridge& engine, std::string ava_cli_path, std::string script_path);

// Folds ScriptRunState::run's progress into EngineBridge::Console() and,
// once the run finishes, into TerminalState::last_run/has_run_result --
// same fields EngineBridge::RunScript() itself sets, so callers that
// read them (e.g. main.cpp's plugin_callbacks.get_last_run_output)
// don't need to know whether the last run happened in-process or via
// StartScriptRun.
//
// IMPORTANT: call this once a frame UNCONDITIONALLY, regardless of
// whether the Terminal panel is currently open -- DrawTerminalPanel
// itself is only called while its tab is open (see main.cpp), so a run
// started while the panel is closed would otherwise sit finished in
// ScriptRunState forever, never folded in, and
// get_last_run_output()/has_run_result would silently go stale.
void PollScriptRun(TerminalState& state, EngineBridge& engine);

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

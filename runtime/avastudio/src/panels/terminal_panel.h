#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "engine/engine_bridge.h"
#include "platform/interfaces/IProcessStream.h"

namespace studio {

struct EditorState;  // panels/editor_panel.h -- only needed by reference here, see PollScriptRun.

struct ScriptRunState {
    std::atomic<bool> running{false};
    std::thread worker;

    std::mutex mutex;
    std::string log;
    std::string::size_type log_forwarded_upto = 0;
    bool has_result = false;
    bool result_consumed = false;
    bool launch_failed = false;
    int exit_code = -1;

    // Set once (from the worker thread, via ExecuteStreaming's on_started
    // callback -- see WinProcess::ExecuteStreaming) as soon as ava_cli.exe
    // has actually launched; null before that and after the run finishes.
    // Guarded by `mutex` above like the rest of this struct's fields --
    // SubmitScriptInput() (called from the UI thread when the user
    // presses Enter in the console's input box) reads it under the same
    // lock the worker thread wrote it under, so there's no race between
    // "script just started" and "user already typed something".
    avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> stdin_writer;
};

struct TerminalState {
    std::string input_buffer;
    bool has_run_result = false;
    RunResult last_run;

    int selection_anchor = -1;
    int selection_cursor = -1;

    ScriptRunState run;
};

void StartScriptRun(TerminalState& state, EngineBridge& engine, std::string ava_cli_path, std::string script_path);

// Also highlights the failing line/column in the editor (via HighlightError)
// when ava_cli's output includes a parseable "error at <source>:<line>[:<col>]:
// <message>" line -- see ParseErrorLocation in terminal_panel.cpp for why this
// subprocess-based run path needs to recover that information from text
// instead of reading it off the VM directly like EngineBridge::RunScript does.
void PollScriptRun(TerminalState& state, EngineBridge& engine, EditorState& editor_state);

// Called when the user presses Enter in the Terminal panel's console
// input box (see DrawTerminalPanel). Echoes `text` into the shared
// console the same way SubmitConsoleInput always did, and -- new -- also
// forwards it to the running script's actual stdin if one is running and
// has reached the point of offering a writer (state.run.stdin_writer),
// which is what actually lets a blocked `input()` call in the script see
// it. Safe to call even when nothing is running (the forward is just
// skipped) or when the script isn't currently blocked on input() (the
// line sits in ava_cli's stdin pipe buffer for whenever it next reads).
void SubmitScriptInput(TerminalState& state, EngineBridge& engine, const std::string& text);

struct TerminalFileClickRequest {
    std::string file_path;
    int line = 0;
    int column = 0;
    std::string message;
};

std::optional<TerminalFileClickRequest> DrawTerminalPanel(TerminalState& state, EngineBridge& engine,
                                                            bool* p_open = nullptr);

}

#include "panels/terminal_panel.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"
#include "panels/editor_panel.h"
#include "util/i18n.h"

namespace studio {

namespace {

// ava_cli (runtime/avacli/src/main.cpp, PrintFormattedError) prints failures
// to stderr as either:
//   error at <source>:<line>:<column>: <message>   (compile errors -- column known)
//   error at <source>:<line>: <message>             (runtime errors -- MakeFrameError
//                                                     only tracks the line, see vm_errors.cpp)
// That text ends up in ScriptRunState::log (captured verbatim from the child
// process), same as everything else the script prints. Unlike the in-process
// engine.RunScript() path (EngineBridge::RunScript, used by perform_run/the
// plugin "run project" callback), which reads line/column/source straight off
// the VM via ava_last_error_*(), the ava_cli-subprocess path this file drives
// has no such API to call -- the only place that information still exists is
// in this printed line. Without parsing it back out, PollScriptRun has no way
// to call HighlightError, which is why F5/the Run button (both go through
// StartScriptRun, not perform_run) never highlighted anything in the editor.
bool ParseErrorLocation(const std::string& log, std::string& out_source, int& out_line,
                         int& out_column, std::string& out_message) {
    const std::string prefix = "error at ";
    size_t start = log.find(prefix);
    if (start == std::string::npos) return false;
    size_t pos = start + prefix.size();

    // The separator we want is the ':' right before the line number, i.e. the
    // first ':' that is followed by a digit -- NOT just the first ':' in the
    // string. On Windows, `source` itself is often an absolute path like
    // "C:\Users\...\script.ava", whose drive-letter colon is followed by '\\'
    // (not a digit) and would otherwise be mistaken for the separator,
    // truncating `source` down to just "C".
    size_t colon1 = std::string::npos;
    for (size_t i = pos; i + 1 < log.size(); ++i) {
        if (log[i] == ':' && std::isdigit(static_cast<unsigned char>(log[i + 1]))) {
            colon1 = i;
            break;
        }
    }
    if (colon1 == std::string::npos) return false;
    std::string source = log.substr(pos, colon1 - pos);
    if (source.empty()) return false;

    size_t num_start = colon1 + 1;
    size_t num_end = num_start;
    while (num_end < log.size() && std::isdigit(static_cast<unsigned char>(log[num_end]))) ++num_end;
    if (num_end == num_start) return false;
    int line = std::atoi(log.substr(num_start, num_end - num_start).c_str());
    if (line <= 0) return false;

    int column = 0;
    size_t after_line = num_end;
    if (after_line < log.size() && log[after_line] == ':') {
        size_t col_start = after_line + 1;
        size_t col_end = col_start;
        while (col_end < log.size() && std::isdigit(static_cast<unsigned char>(log[col_end]))) ++col_end;
        if (col_end > col_start) {
            column = std::atoi(log.substr(col_start, col_end - col_start).c_str());
            after_line = col_end;
        }
    }

    // Expect ": " then the message, up to end of line.
    if (after_line + 1 >= log.size() || log[after_line] != ':') return false;
    size_t msg_start = after_line + 1;
    if (msg_start < log.size() && log[msg_start] == ' ') ++msg_start;
    size_t msg_end = log.find('\n', msg_start);
    if (msg_end == std::string::npos) msg_end = log.size();

    out_source = source;
    out_line = line;
    out_column = column;
    out_message = log.substr(msg_start, msg_end - msg_start);
    return true;
}

}  // namespace

void StartScriptRun(TerminalState& state, EngineBridge& engine, std::string ava_cli_path, std::string script_path) {
    ScriptRunState& run = state.run;
    if (run.running.load()) return;
    if (run.worker.joinable()) run.worker.join();

    engine.AppendConsoleLine(ConsoleLine::Kind::Info,
                              "Run " + (script_path.empty() ? std::string("<script>") : script_path));

    {
        std::lock_guard<std::mutex> lock(run.mutex);
        run.log.clear();
        run.log_forwarded_upto = 0;
        run.has_result = false;
        run.result_consumed = false;
        run.launch_failed = false;
        run.exit_code = -1;
        run.stdin_writer.reset();
    }
    run.running = true;

    run.worker = std::thread([&run, ava_cli_path = std::move(ava_cli_path),
                               script_path = std::move(script_path)]() {
        auto platform = ava::platform::Platform::Create();
        ava::platform::IProcess& process = platform->Process();
        auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);

        bool launched = false;
        int exit_code = -1;
        std::vector<std::string> args{script_path};

        if (streaming) {
            launched = streaming->ExecuteStreaming(
                ava_cli_path, args,
                [&run](const std::string& chunk) {
                    std::lock_guard<std::mutex> lock(run.mutex);
                    run.log += chunk;
                },
                exit_code,
                [&run](avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> writer) {
                    std::lock_guard<std::mutex> lock(run.mutex);
                    run.stdin_writer = std::move(writer);
                });
        } else {
            ava::platform::ProcessResult result;
            launched = process.Execute(ava_cli_path, args, result);
            if (launched) {
                std::lock_guard<std::mutex> lock(run.mutex);
                run.log = result.stdout_output;
                if (!result.stderr_output.empty()) {
                    if (!run.log.empty()) run.log += "\n";
                    run.log += result.stderr_output;
                }
                exit_code = result.exit_code;
            }
        }

        std::lock_guard<std::mutex> lock(run.mutex);
        if (!launched) {
            run.log = "error: could not run '" + ava_cli_path +
                       "' -- check the ava_cli path under Build > Advanced.\n";
            run.launch_failed = true;
            run.exit_code = -1;
        } else {
            run.exit_code = exit_code;
        }
        // The child is gone by this point (ExecuteStreaming only returns after
        // WaitForSingleObject) and WinProcess has already closed its end of the pipe --
        // drop our reference too so DrawTerminalPanel/SubmitScriptInput stop trying to
        // write to a dead child instead of relying on every WriteLine() call noticing.
        run.stdin_writer.reset();
        run.has_result = true;
        run.running = false;
    });
}

void PollScriptRun(TerminalState& state, EngineBridge& engine, EditorState& editor_state) {
    ScriptRunState& run = state.run;

    std::string new_output;
    bool finished_now = false;
    bool launch_failed = false;
    int exit_code = -1;
    std::string full_log;
    {
        std::lock_guard<std::mutex> lock(run.mutex);
        if (run.log.size() > run.log_forwarded_upto) {
            new_output = run.log.substr(run.log_forwarded_upto);
            run.log_forwarded_upto = run.log.size();
        }
        if (run.has_result && !run.result_consumed) {
            finished_now = true;
            launch_failed = run.launch_failed;
            exit_code = run.exit_code;
            run.result_consumed = true;
            full_log = run.log;
        }
    }

    if (!new_output.empty()) engine.AppendExternalOutput(new_output);

    if (finished_now) {
        engine.FlushExternalOutput();

        RunResult result;
        if (launch_failed) {

            result.success = false;
            result.message = "could not launch ava_cli -- see message above";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        } else if (exit_code == 0) {
            result.success = true;
            result.message = "OK";
            engine.AppendConsoleLine(ConsoleLine::Kind::Success, result.message);
        } else if (exit_code == 1) {

            result.success = false;
            // ava_cli already printed the precise "error at <source>:<line>[:<col>]: <msg>"
            // line as part of the streamed output above -- pull line/column/source back
            // out of it so this failure behaves like the in-process run path: clickable
            // in the console AND auto-highlighted in the editor, instead of a dead-end
            // "see output above" pointer.
            std::string parsed_source, parsed_message;
            int parsed_line = 0, parsed_column = 0;
            if (ParseErrorLocation(full_log, parsed_source, parsed_line, parsed_column, parsed_message)) {
                result.error_source = parsed_source;
                result.error_line = parsed_line;
                result.error_column = parsed_column;
                result.message = parsed_message;
                engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message, parsed_source,
                                          parsed_line, parsed_column);
                studio::HighlightError(editor_state, parsed_source, parsed_line, parsed_column,
                                        parsed_message);
            } else {
                result.message = "script exited with an error -- see output above";
                engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
            }
        } else {

            result.success = false;
            result.message = "the script crashed the process it ran in (exit code " +
                              std::to_string(exit_code) + ") -- likely a native/extern call";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        }
        state.last_run = result;
        state.has_run_result = true;
    }
}

void SubmitScriptInput(TerminalState& state, EngineBridge& engine, const std::string& text) {
    // Echo into the shared console first, same as before -- this part
    // never depended on a script actually being able to read it.
    engine.SubmitConsoleInput(text);

    // ...and now actually feed it to the running script, if there is one
    // and it's reached the point of handing us a writer (see
    // StartScriptRun's on_started callback above). Without this, the
    // Enter above only ever echoed into the console -- nothing read it,
    // which is the bug this whole file exists to fix.
    avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> writer;
    {
        std::lock_guard<std::mutex> lock(state.run.mutex);
        writer = state.run.stdin_writer;
    }
    if (writer) writer->WriteLine(text);
}

namespace {

ImVec4 ColorForKind(ConsoleLine::Kind kind) {
    using studio::palette::FromHex;
    switch (kind) {
        case ConsoleLine::Kind::Info:    return FromHex(palette::kTextMuted);
        case ConsoleLine::Kind::Stdout:  return FromHex(palette::kTextPrimary);
        case ConsoleLine::Kind::Error:   return FromHex(palette::kError);
        case ConsoleLine::Kind::Success: return FromHex(palette::kSuccess);
        case ConsoleLine::Kind::Input:   return FromHex(palette::kInfo);
    }
    return FromHex(palette::kTextPrimary);
}

const char* PrefixForKind(ConsoleLine::Kind kind) {
    switch (kind) {
        case ConsoleLine::Kind::Info:    return "> ";
        case ConsoleLine::Kind::Stdout:  return "";
        case ConsoleLine::Kind::Error:   return "";
        case ConsoleLine::Kind::Success: return "";
        case ConsoleLine::Kind::Input:   return "";
    }
    return "";
}

std::string LineText(const ConsoleLine& line) {
    return std::string(PrefixForKind(line.kind)) + line.text;
}

std::string JoinLines(const std::vector<ConsoleLine>& console, int first, int last) {
    std::string out;
    for (int i = first; i <= last; ++i) {
        if (i > first) out += '\n';
        out += LineText(console[static_cast<size_t>(i)]);
    }
    return out;
}

void CopySelection(TerminalState& state, const std::vector<ConsoleLine>& console) {
    if (state.selection_anchor < 0 || state.selection_cursor < 0) return;
    const int first = std::min(state.selection_anchor, state.selection_cursor);
    const int last = std::max(state.selection_anchor, state.selection_cursor);
    ImGui::SetClipboardText(JoinLines(console, first, last).c_str());
}

void CopyAll(const std::vector<ConsoleLine>& console) {
    if (console.empty()) return;
    ImGui::SetClipboardText(JoinLines(console, 0, static_cast<int>(console.size()) - 1).c_str());
}

}

std::optional<TerminalFileClickRequest> DrawTerminalPanel(TerminalState& state, EngineBridge& engine,
                                                            bool* p_open) {
    std::optional<TerminalFileClickRequest> click_request;

    ImGui::Begin((util::Tr("panel.terminal.title") + "###terminal").c_str(), p_open);

    const auto& console = engine.Console();

    const int line_count = static_cast<int>(console.size());
    if (state.selection_anchor >= line_count || state.selection_cursor >= line_count) {
        state.selection_anchor = state.selection_cursor = -1;
    }

    const float row_right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::TextDisabled("%s", util::Tr("terminal.section_label").c_str());
    ImGui::SameLine();

    const std::string copy_console_label = util::Tr("terminal.copy_console");
    const std::string clear_label = util::Tr("common.clear");
    const float clear_w = ImGui::CalcTextSize(clear_label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float copy_w = ImGui::CalcTextSize(copy_console_label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float button_gap = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const float clear_x = row_right_x - clear_w;
    const float copy_x = clear_x - button_gap - copy_w;

    if (ImGui::GetCursorPosX() < copy_x) {
        ImGui::SetCursorPosX(copy_x);
    }
    if (ImGui::SmallButton(copy_console_label.c_str())) {
        CopyAll(console);
    }

    ImGui::SameLine(0.0f, button_gap);
    if (ImGui::GetCursorPosX() < clear_x) {
        ImGui::SetCursorPosX(clear_x);
    }
    if (ImGui::SmallButton(clear_label.c_str())) {
        engine.ClearConsole();
        state.selection_anchor = state.selection_cursor = -1;
    }

    ImGui::Separator();

    float input_row_height = ImGui::GetFrameHeightWithSpacing();
    ImVec2 console_size = ImVec2(0, -input_row_height);

    ImGui::BeginChild("console_scrollback", console_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    if (console.empty()) {
        ImGui::TextDisabled("%s", util::Tr("terminal.empty").c_str());
    } else {
        for (int i = 0; i < line_count; ++i) {
            const ConsoleLine& line = console[static_cast<size_t>(i)];
            ImGui::PushID(i);

            const int first = std::min(state.selection_anchor, state.selection_cursor);
            const int last = std::max(state.selection_anchor, state.selection_cursor);
            const bool is_selected = state.selection_anchor >= 0 && i >= first && i <= last;

            ImGui::PushStyleColor(ImGuiCol_Text, ColorForKind(line.kind));
            const std::string text = LineText(line);
            ImGui::Selectable(text.c_str(), is_selected);
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift && state.selection_anchor >= 0) {
                    state.selection_cursor = i;
                } else {
                    state.selection_anchor = state.selection_cursor = i;
                }

                if (line.kind == ConsoleLine::Kind::Error && line.error_line != 0) {
                    click_request = TerminalFileClickRequest{line.error_source, line.error_line,
                                                            line.error_column, line.text};
                }
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                state.selection_anchor >= 0) {
                state.selection_cursor = i;
            }

            ImGui::PopID();
        }
    }

    if (ImGui::BeginPopupContextWindow("##console_context")) {
        const bool has_selection = state.selection_anchor >= 0 && state.selection_cursor >= 0;
        if (ImGui::MenuItem(util::Tr("common.copy").c_str(), "Ctrl+C", false, has_selection)) {
            CopySelection(state, console);
        }
        if (ImGui::MenuItem(util::Tr("common.copy_all").c_str(), nullptr, false, !console.empty())) {
            CopyAll(console);
        }
        if (ImGui::MenuItem(util::Tr("common.select_all").c_str(), "Ctrl+A", false, !console.empty())) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowFocused()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            CopySelection(state, console);
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false) && !console.empty()) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
    }

    static size_t last_seen_line_count = 0;
    bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    if (console.size() != last_seen_line_count) {
        last_seen_line_count = console.size();
        if (was_at_bottom) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild();

    ImGui::PushItemWidth(-1);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    bool submitted = ImGui::InputTextWithHint(
        "##console_input", util::Tr("terminal.input_hint").c_str(),
        &state.input_buffer, flags);
    ImGui::PopItemWidth();

    if (submitted && !state.input_buffer.empty()) {
        SubmitScriptInput(state, engine, state.input_buffer);
        state.input_buffer.clear();
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();

    return click_request;
}

}

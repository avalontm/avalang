#include "panels/terminal_panel.h"

#include <algorithm>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"

namespace studio {

// --- background script run (out-of-process, via ava_cli.exe) ---------
// Same shape as build_panel.cpp's StartBuild -- see ScriptRunState's
// comment in terminal_panel.h for why Run works this way.
void StartScriptRun(TerminalState& state, EngineBridge& engine, std::string ava_cli_path, std::string script_path) {
    ScriptRunState& run = state.run;
    if (run.running.load()) return;              // one run at a time
    if (run.worker.joinable()) run.worker.join(); // previous run already finished, just reap it

    // Same "Run <path>" marker EngineBridge::RunScript() itself pushes,
    // written here (synchronously, on the UI thread that's calling
    // StartScriptRun) so it shows up right away instead of waiting for
    // the child process to produce its first byte of output.
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
                exit_code);
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
        run.has_result = true;
        run.running = false;
    });
}

void PollScriptRun(TerminalState& state, EngineBridge& engine) {
    ScriptRunState& run = state.run;

    std::string new_output;
    bool finished_now = false;
    bool launch_failed = false;
    int exit_code = -1;
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
        }
    }

    if (!new_output.empty()) engine.AppendExternalOutput(new_output);

    if (finished_now) {
        engine.FlushExternalOutput();

        RunResult result;
        if (launch_failed) {
            // ava_cli_path itself couldn't be started -- not a script
            // problem at all, most likely a misconfigured/missing
            // ava_cli(.exe). The explanatory line is already in the
            // console via AppendExternalOutput above (it's what
            // StartScriptRun wrote into run.log).
            result.success = false;
            result.message = "could not launch ava_cli -- see message above";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        } else if (exit_code == 0) {
            result.success = true;
            result.message = "OK";
            engine.AppendConsoleLine(ConsoleLine::Kind::Success, result.message);
        } else if (exit_code == 1) {
            // Normal compile/runtime error -- ava_cli.exe already printed
            // "compile error: ..." / "runtime error: ..." above via
            // stderr (see runtime/avacli/src/main.cpp), so the console
            // already shows the actual message. This just marks the run
            // as failed; no error_line/column, ava_cli doesn't expose
            // those today (see ScriptRunState's comment on the trade-off).
            result.success = false;
            result.message = "script exited with an error -- see output above";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        } else {
            // Neither a clean exit nor a normal script-level error --
            // the child process itself died (e.g. a native access
            // violation from an unsafe extern binding). AvaStudio is
            // still alive to say so, which is the whole point.
            result.success = false;
            result.message = "the script crashed the process it ran in (exit code " +
                              std::to_string(exit_code) + ") -- likely a native/extern call";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        }
        state.last_run = result;
        state.has_run_result = true;
    }
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

// Joins console lines [first, last] (inclusive, both valid indices) into
// one clipboard-ready string, one ConsoleLine per output line (an error's
// embedded '\n' source excerpt stays inside its own single entry).
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

} // namespace

std::optional<TerminalFileClickRequest> DrawTerminalPanel(TerminalState& state, EngineBridge& engine,
                                                            bool* p_open) {
    std::optional<TerminalFileClickRequest> click_request;

    ImGui::Begin("Terminal", p_open);

    const auto& console = engine.Console();

    // A stale selection can outlive the lines it points to (Clear, or a
    // run that replaces the scrollback) -- drop it rather than let a
    // future frame index off the end of a now-smaller console.
    const int line_count = static_cast<int>(console.size());
    if (state.selection_anchor >= line_count || state.selection_cursor >= line_count) {
        state.selection_anchor = state.selection_cursor = -1;
    }

    // Capture the row's right edge before drawing anything on it, so the
    // Clear button below can be pinned flush to it regardless of how wide
    // the panel currently is.
    const float row_right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::TextDisabled("Execution console");
    ImGui::SameLine();
    // Pin "Clear" (and "Copy console" just to its left, with a bit of
    // breathing room between them) to the row's right edge instead of
    // floating right after the label -- lines up with every other panel's
    // top-right action.
    const float clear_w = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float copy_w = ImGui::CalcTextSize("Copy console").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float button_gap = ImGui::GetStyle().ItemSpacing.x * 2.0f; // a little extra separation between the two buttons
    const float clear_x = row_right_x - clear_w;
    const float copy_x = clear_x - button_gap - copy_w;

    if (ImGui::GetCursorPosX() < copy_x) {
        ImGui::SetCursorPosX(copy_x);
    }
    if (ImGui::SmallButton("Copy console")) {
        CopyAll(console);
    }

    ImGui::SameLine(0.0f, button_gap);
    if (ImGui::GetCursorPosX() < clear_x) {
        ImGui::SetCursorPosX(clear_x);
    }
    if (ImGui::SmallButton("Clear")) {
        engine.ClearConsole();
        state.selection_anchor = state.selection_cursor = -1;
    }

    ImGui::Separator();

    // Leave room below the scrollback for a separator + one input line.
    float input_row_height = ImGui::GetFrameHeightWithSpacing();
    ImVec2 console_size = ImVec2(0, -input_row_height);

    ImGui::BeginChild("console_scrollback", console_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    if (console.empty()) {
        ImGui::TextDisabled("Press Run (F5) to compile and run the editor's script.");
    } else {
        for (int i = 0; i < line_count; ++i) {
            const ConsoleLine& line = console[static_cast<size_t>(i)];
            ImGui::PushID(i);

            const int first = std::min(state.selection_anchor, state.selection_cursor);
            const int last = std::max(state.selection_anchor, state.selection_cursor);
            const bool is_selected = state.selection_anchor >= 0 && i >= first && i <= last;

            // Selectable (not TextUnformatted) so each line can carry a
            // selection highlight and be click/shift-click/drag-selected
            // like a real console -- text still gets colored per-kind via
            // the same PushStyleColor the old plain-text version used.
            // TextUnformatted (not Text/"%s") because compile-error
            // messages can contain embedded '\n's (source excerpt + "^"
            // column caret, see core/src/frontend/frontend_antlr.cpp) --
            // Selectable's label handles that the same way TextUnformatted
            // did, rendering every line instead of collapsing them.
            ImGui::PushStyleColor(ImGuiCol_Text, ColorForKind(line.kind));
            const std::string text = LineText(line);
            ImGui::Selectable(text.c_str(), is_selected);
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift && state.selection_anchor >= 0) {
                    state.selection_cursor = i; // extend the existing selection
                } else {
                    state.selection_anchor = state.selection_cursor = i; // start a new one
                }
                // An Error line with a known source position (see
                // ConsoleLine::error_line in engine_bridge.h) is
                // clickable like a terminal's problem matcher: jump
                // straight to the offending file/line instead of only
                // selecting the text. Falls back to nothing (just
                // selects) for lines with no position, e.g. an error
                // predating source-line tracking.
                if (line.kind == ConsoleLine::Kind::Error && line.error_line != 0) {
                    click_request = TerminalFileClickRequest{line.error_source, line.error_line,
                                                            line.error_column, line.text};
                }
            }
            // Dragging with the button still held extends the selection
            // to whatever line the mouse is over, same as a normal text
            // drag-select.
            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                state.selection_anchor >= 0) {
                state.selection_cursor = i;
            }

            ImGui::PopID();
        }
    }

    // Right-click anywhere in the scrollback for Copy / Copy All / Select
    // All -- same idiom as a terminal, and the one place Ctrl+C-averse
    // users can still get the text out.
    if (ImGui::BeginPopupContextWindow("##console_context")) {
        const bool has_selection = state.selection_anchor >= 0 && state.selection_cursor >= 0;
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, has_selection)) {
            CopySelection(state, console);
        }
        if (ImGui::MenuItem("Copy All", nullptr, false, !console.empty())) {
            CopyAll(console);
        }
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, !console.empty())) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
        ImGui::EndPopup();
    }

    // Keyboard shortcuts, gated on the scrollback itself having focus so
    // Ctrl+C/Ctrl+A here don't steal those keys from the code editor or
    // anywhere else while this panel merely happens to be visible.
    if (ImGui::IsWindowFocused()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            CopySelection(state, console);
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false) && !console.empty()) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
    }

    // Auto-scroll to the bottom on new output, but only if the user was
    // already at (or near) the bottom -- so scrolling up to read earlier
    // output doesn't get yanked back down by the next print().
    static size_t last_seen_line_count = 0;
    bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    if (console.size() != last_seen_line_count) {
        last_seen_line_count = console.size();
        if (was_at_bottom) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild();

    // --- Console input -------------------------------------------------
    // Scaffolding for a future `input()` builtin -- see the long comment
    // on EngineBridge::SubmitConsoleInput() for why this can't actually
    // feed a running script yet. Kept enabled (not greyed out) and fully
    // wired to echo + queue, so wiring up real input() later is just
    // making something *read* input_queue_, not building this UI.
    ImGui::PushItemWidth(-1);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    bool submitted = ImGui::InputTextWithHint(
        "##console_input", "Type something and press Enter (input() doesn't exist in the language yet)...",
        &state.input_buffer, flags);
    ImGui::PopItemWidth();

    if (submitted && !state.input_buffer.empty()) {
        engine.SubmitConsoleInput(state.input_buffer);
        state.input_buffer.clear();
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();

    return click_request;
}

} // namespace studio
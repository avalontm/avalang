#include "panels/terminal_panel.h"

#include <algorithm>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"
#include "util/i18n.h"

namespace studio {

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

            result.success = false;
            result.message = "could not launch ava_cli -- see message above";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
        } else if (exit_code == 0) {
            result.success = true;
            result.message = "OK";
            engine.AppendConsoleLine(ConsoleLine::Kind::Success, result.message);
        } else if (exit_code == 1) {

            result.success = false;
            result.message = "script exited with an error -- see output above";
            engine.AppendConsoleLine(ConsoleLine::Kind::Error, result.message);
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
        engine.SubmitConsoleInput(state.input_buffer);
        state.input_buffer.clear();
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();

    return click_request;
}

}

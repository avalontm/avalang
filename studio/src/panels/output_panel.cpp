#include "panels/output_panel.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"

namespace studio {

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

} // namespace

void DrawOutputPanel(OutputState& state, EngineBridge& engine) {
    ImGui::Begin("Output");

    const auto& console = engine.Console();

    if (ImGui::SmallButton("Clear")) {
        engine.ClearConsole();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Execution console");

    ImGui::Separator();

    // Leave room below the scrollback for a separator + one input line.
    float input_row_height = ImGui::GetFrameHeightWithSpacing();
    ImVec2 console_size = ImVec2(0, -input_row_height);

    ImGui::BeginChild("console_scrollback", console_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    if (console.empty()) {
        ImGui::TextDisabled("Press Run (F5) to compile and run the editor's script.");
    } else {
        for (const ConsoleLine& line : console) {
            ImGui::PushStyleColor(ImGuiCol_Text, ColorForKind(line.kind));
            // TextUnformatted (not Text/"%s") because compile-error
            // messages can contain embedded '\n's (source excerpt + "^"
            // column caret, see core/src/frontend/frontend_antlr.cpp) --
            // this renders each of those lines instead of collapsing
            // them, and also sidesteps printf-style escaping of "%" in
            // script-provided text.
            std::string prefixed = std::string(PrefixForKind(line.kind)) + line.text;
            ImGui::TextUnformatted(prefixed.c_str());
            ImGui::PopStyleColor();
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
}

} // namespace studio

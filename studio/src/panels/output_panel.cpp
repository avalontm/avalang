#include "panels/output_panel.h"

#include <algorithm>

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

void CopySelection(OutputState& state, const std::vector<ConsoleLine>& console) {
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

void DrawOutputPanel(OutputState& state, EngineBridge& engine) {
    ImGui::Begin("Output");

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
}

} // namespace studio
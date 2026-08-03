#include "panels/logs_panel.h"

#include <algorithm>

#include "imgui.h"
#include "palette.h"

namespace studio {

namespace {

std::string JoinLines(const std::vector<LogLine>& lines, int first, int last) {
    std::string out;
    for (int i = first; i <= last; ++i) {
        if (i > first) out += '\n';
        out += lines[static_cast<size_t>(i)].text;
    }
    return out;
}

void CopySelection(LogsState& state, const std::vector<LogLine>& lines) {
    if (state.selection_anchor < 0 || state.selection_cursor < 0) return;
    const int first = std::min(state.selection_anchor, state.selection_cursor);
    const int last = std::max(state.selection_anchor, state.selection_cursor);
    ImGui::SetClipboardText(JoinLines(lines, first, last).c_str());
}

void CopyAll(const std::vector<LogLine>& lines) {
    if (lines.empty()) return;
    ImGui::SetClipboardText(JoinLines(lines, 0, static_cast<int>(lines.size()) - 1).c_str());
}

} // namespace

void DrawLogsPanel(LogsState& state, LogBridge& log_bridge, bool* p_open) {
    ImGui::Begin("Output", p_open);

    const auto& lines = log_bridge.Lines();

    const int line_count = static_cast<int>(lines.size());
    if (state.selection_anchor >= line_count || state.selection_cursor >= line_count) {
        state.selection_anchor = state.selection_cursor = -1;
    }

    const float row_right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::TextDisabled("General logs");
    ImGui::SameLine();
    const float clear_w = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float copy_w = ImGui::CalcTextSize("Copy all").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float button_gap = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const float clear_x = row_right_x - clear_w;
    const float copy_x = clear_x - button_gap - copy_w;

    if (ImGui::GetCursorPosX() < copy_x) {
        ImGui::SetCursorPosX(copy_x);
    }
    if (ImGui::SmallButton("Copy all")) {
        CopyAll(lines);
    }

    ImGui::SameLine(0.0f, button_gap);
    if (ImGui::GetCursorPosX() < clear_x) {
        ImGui::SetCursorPosX(clear_x);
    }
    if (ImGui::SmallButton("Clear")) {
        log_bridge.Clear();
        state.selection_anchor = state.selection_cursor = -1;
    }

    ImGui::Separator();

    ImGui::BeginChild("logs_scrollback", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (lines.empty()) {
        ImGui::TextDisabled("Plugin and host events will show up here.");
    } else {
        for (int i = 0; i < line_count; ++i) {
            const LogLine& line = lines[static_cast<size_t>(i)];
            ImGui::PushID(i);

            const int first = std::min(state.selection_anchor, state.selection_cursor);
            const int last = std::max(state.selection_anchor, state.selection_cursor);
            const bool is_selected = state.selection_anchor >= 0 && i >= first && i <= last;

            ImGui::PushStyleColor(ImGuiCol_Text, studio::palette::FromHex(palette::kTextMuted));
            ImGui::Selectable(line.text.c_str(), is_selected);
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift && state.selection_anchor >= 0) {
                    state.selection_cursor = i;
                } else {
                    state.selection_anchor = state.selection_cursor = i;
                }
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                state.selection_anchor >= 0) {
                state.selection_cursor = i;
            }

            ImGui::PopID();
        }
    }

    if (ImGui::BeginPopupContextWindow("##logs_context")) {
        const bool has_selection = state.selection_anchor >= 0 && state.selection_cursor >= 0;
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, has_selection)) {
            CopySelection(state, lines);
        }
        if (ImGui::MenuItem("Copy All", nullptr, false, !lines.empty())) {
            CopyAll(lines);
        }
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, !lines.empty())) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowFocused()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            CopySelection(state, lines);
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false) && !lines.empty()) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
    }

    static size_t last_seen_line_count = 0;
    bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    if (lines.size() != last_seen_line_count) {
        last_seen_line_count = lines.size();
        if (was_at_bottom) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace studio

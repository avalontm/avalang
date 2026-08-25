#include "panels/problems_panel.h"

#include <algorithm>

#include "imgui.h"
#include "palette.h"
#include "util/i18n.h"

namespace studio {

void UpdateProblemsFromResult(ProblemsState& state, const std::string& source_label, const RunResult& result,
                               const std::string& fallback_file) {
    auto& entries = state.entries;
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                  [&](const ProblemEntry& e) { return e.source_label == source_label; }),
                  entries.end());

    if (result.success) return;

    ProblemEntry entry;
    entry.source_label = source_label;
    entry.file = result.error_source.empty() ? fallback_file : result.error_source;
    entry.line = result.error_line;
    entry.column = result.error_column;
    entry.message = result.message;
    entries.push_back(std::move(entry));

    state.selection_anchor = state.selection_cursor = -1;
}

namespace {

std::string TrFormat(const std::string& key, std::initializer_list<std::string> args) {
    std::string result = util::Tr(key);
    for (const std::string& arg : args) {
        const size_t pos = result.find("%s");
        if (pos == std::string::npos) break;
        result = result.substr(0, pos) + arg + result.substr(pos + 2);
    }
    return result;
}

std::string EntryLineText(const ProblemEntry& e) {
    std::string location = e.file.empty() ? std::string("?") : e.file;
    if (e.line > 0) {
        location += ":" + std::to_string(e.line);
        if (e.column > 0) location += ":" + std::to_string(e.column);
    }
    return location + " -- " + e.message;
}

std::string JoinEntries(const std::vector<ProblemEntry>& entries, int first, int last) {
    std::string out;
    for (int i = first; i <= last; ++i) {
        if (i > first) out += '\n';
        out += EntryLineText(entries[static_cast<size_t>(i)]);
    }
    return out;
}

void CopySelection(ProblemsState& state) {
    if (state.selection_anchor < 0 || state.selection_cursor < 0) return;
    const int first = std::min(state.selection_anchor, state.selection_cursor);
    const int last = std::max(state.selection_anchor, state.selection_cursor);
    ImGui::SetClipboardText(JoinEntries(state.entries, first, last).c_str());
}

void CopyAll(const std::vector<ProblemEntry>& entries) {
    if (entries.empty()) return;
    ImGui::SetClipboardText(JoinEntries(entries, 0, static_cast<int>(entries.size()) - 1).c_str());
}

}

std::optional<ProblemsFileClickRequest> DrawProblemsPanel(ProblemsState& state, bool* p_open) {
    std::optional<ProblemsFileClickRequest> click_request;

    const int count = static_cast<int>(state.entries.size());
    const std::string title = (count == 0 ? util::Tr("panel.problems.title")
                                           : TrFormat("panel.problems.title_with_count", {std::to_string(count)})) +
                               "###problems";
    ImGui::Begin(title.c_str(), p_open);

    const auto& entries = state.entries;
    const int line_count = static_cast<int>(entries.size());
    if (state.selection_anchor >= line_count || state.selection_cursor >= line_count) {
        state.selection_anchor = state.selection_cursor = -1;
    }

    const float row_right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::TextDisabled("%s", util::Tr("problems.section_label").c_str());
    ImGui::SameLine();
    const std::string copy_all_label = util::Tr("common.copy_all");
    const float copy_w = ImGui::CalcTextSize(copy_all_label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float copy_x = row_right_x - copy_w;
    if (ImGui::GetCursorPosX() < copy_x) {
        ImGui::SetCursorPosX(copy_x);
    }
    if (ImGui::SmallButton(copy_all_label.c_str())) {
        CopyAll(entries);
    }

    ImGui::Separator();

    ImGui::BeginChild("problems_scrollback", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (entries.empty()) {
        ImGui::TextDisabled("%s", util::Tr("problems.empty").c_str());
    } else {
        for (int i = 0; i < line_count; ++i) {
            const ProblemEntry& entry = entries[static_cast<size_t>(i)];
            ImGui::PushID(i);

            const int first = std::min(state.selection_anchor, state.selection_cursor);
            const int last = std::max(state.selection_anchor, state.selection_cursor);
            const bool is_selected = state.selection_anchor >= 0 && i >= first && i <= last;

            ImGui::PushStyleColor(ImGuiCol_Text, palette::FromHex(palette::kError));
            const std::string text = EntryLineText(entry);
            ImGui::Selectable(text.c_str(), is_selected);
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift && state.selection_anchor >= 0) {
                    state.selection_cursor = i;
                } else {
                    state.selection_anchor = state.selection_cursor = i;
                }
                if (entry.line != 0) {
                    click_request = ProblemsFileClickRequest{entry.file, entry.line, entry.column, entry.message};
                }
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                state.selection_anchor >= 0) {
                state.selection_cursor = i;
            }

            ImGui::PopID();
        }
    }

    if (ImGui::BeginPopupContextWindow("##problems_context")) {
        const bool has_selection = state.selection_anchor >= 0 && state.selection_cursor >= 0;
        if (ImGui::MenuItem(util::Tr("common.copy").c_str(), "Ctrl+C", false, has_selection)) {
            CopySelection(state);
        }
        if (ImGui::MenuItem(util::Tr("common.copy_all").c_str(), nullptr, false, !entries.empty())) {
            CopyAll(entries);
        }
        if (ImGui::MenuItem(util::Tr("common.select_all").c_str(), "Ctrl+A", false, !entries.empty())) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowFocused()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            CopySelection(state);
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false) && !entries.empty()) {
            state.selection_anchor = 0;
            state.selection_cursor = line_count - 1;
        }
    }

    ImGui::EndChild();
    ImGui::End();

    return click_request;
}

}

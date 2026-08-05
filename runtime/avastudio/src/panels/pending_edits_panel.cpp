#include "panels/pending_edits_panel.h"

#include "imgui.h"
#include "palette.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace studio {

namespace {

// One line of a computed diff -- see ComputeLineDiff below.
struct DiffLine {
    enum class Kind { Context, Added, Removed };
    Kind kind;
    std::string text;
};

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    // std::getline drops the trailing '\n' that separates each line but
    // also silently drops the *file's* final line if it has no trailing
    // newline of its own -- rebuilding line-by-line like this is fine
    // either way for a review diff (a missing final blank line is not
    // something the person needs highlighted).
    while (std::getline(stream, line)) lines.push_back(line);
    return lines;
}

// A small LCS-based line diff -- good enough for reviewing an agent's
// proposed edit, not meant to be a general-purpose diff engine. O(n*m)
// in the number of lines on each side, so capped: past kMaxDiffCells
// total table entries, this gives up on a line-level diff and falls
// back to "everything old removed, everything new added" so a huge
// file still renders (just without line-level highlighting) instead of
// hanging the UI thread computing an LCS table.
constexpr size_t kMaxDiffCells = 4'000'000; // e.g. ~2000 lines x ~2000 lines

std::vector<DiffLine> ComputeLineDiff(const std::string& old_content, const std::string& new_content) {
    std::vector<std::string> a = SplitLines(old_content);
    std::vector<std::string> b = SplitLines(new_content);
    const size_t n = a.size();
    const size_t m = b.size();

    std::vector<DiffLine> result;

    if (n * m > kMaxDiffCells) {
        for (const auto& line : a) result.push_back({DiffLine::Kind::Removed, line});
        for (const auto& line : b) result.push_back({DiffLine::Kind::Added, line});
        return result;
    }

    // Standard LCS length table, then backtrack to emit context/added/
    // removed lines in order.
    std::vector<std::vector<int>> lcs(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            lcs[i][j] = (a[i - 1] == b[j - 1]) ? lcs[i - 1][j - 1] + 1 : std::max(lcs[i - 1][j], lcs[i][j - 1]);
        }
    }

    std::vector<DiffLine> reversed;
    size_t i = n, j = m;
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            reversed.push_back({DiffLine::Kind::Context, a[i - 1]});
            --i;
            --j;
        } else if (lcs[i - 1][j] >= lcs[i][j - 1]) {
            reversed.push_back({DiffLine::Kind::Removed, a[i - 1]});
            --i;
        } else {
            reversed.push_back({DiffLine::Kind::Added, b[j - 1]});
            --j;
        }
    }
    while (i > 0) { reversed.push_back({DiffLine::Kind::Removed, a[i - 1]}); --i; }
    while (j > 0) { reversed.push_back({DiffLine::Kind::Added, b[j - 1]}); --j; }

    result.assign(reversed.rbegin(), reversed.rend());
    return result;
}

void DrawDiffLine(const DiffLine& line) {
    using studio::palette::FromHex;
    const char* prefix = "  ";
    ImVec4 color = FromHex(palette::kTextSecondary);
    switch (line.kind) {
        case DiffLine::Kind::Added:
            prefix = "+ ";
            color = FromHex(palette::kSuccess);
            break;
        case DiffLine::Kind::Removed:
            prefix = "- ";
            color = FromHex(palette::kError);
            break;
        case DiffLine::Kind::Context:
            prefix = "  ";
            color = FromHex(palette::kTextMuted);
            break;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted((std::string(prefix) + line.text).c_str());
    ImGui::PopStyleColor();
}

} // namespace

void DrawPendingEditsPanel(PluginHost& plugin_host) {
    std::vector<PendingEdit> edits = plugin_host.PendingEdits();
    if (edits.empty()) return;

    char title[64];
    std::snprintf(title, sizeof(title), "Cambios propuestos (%zu)###pending_edits", edits.size());
    // Not docked (no default_dock_slot -- this isn't a plugin panel, it's
    // host UI) -- a floating window makes more sense for something that
    // pops up rarely and needs a decision before it's dismissed, rather
    // than permanently occupying a dock slot the rest of the session.
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin(title, &open)) {
        ImGui::End();
        return;
    }

    for (const PendingEdit& edit : edits) {
        ImGui::PushID(edit.id);

        ImGui::TextColored(palette::FromHex(palette::kPrimary), "%s", edit.path.c_str());
        if (!edit.description.empty()) {
            ImGui::TextWrapped("%s", edit.description.c_str());
        }

        ImGui::BeginChild("diff", ImVec2(0, 220), true, ImGuiWindowFlags_HorizontalScrollbar);
        std::vector<DiffLine> diff = ComputeLineDiff(edit.old_content, edit.new_content);
        if (diff.empty()) {
            ImGui::TextDisabled("(sin cambios de contenido)");
        } else {
            for (const DiffLine& line : diff) DrawDiffLine(line);
        }
        ImGui::EndChild();

        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kPrimary));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kPrimaryHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(palette::kPrimaryDark));
        if (ImGui::Button("Aplicar")) {
            plugin_host.ApproveEdit(edit.id);
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::Button("Rechazar")) {
            plugin_host.RejectEdit(edit.id);
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::End();
    // Closing the window via its titlebar X rejects every proposal still
    // shown, rather than leaving them queued invisibly -- "nunca se
    // aplica solo" cuts both ways: dismissing the review is a rejection,
    // not a silent approval.
    if (!open) {
        for (const PendingEdit& edit : edits) plugin_host.RejectEdit(edit.id);
    }
}

} // namespace studio

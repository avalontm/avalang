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

struct DiffLine {
    enum class Kind { Context, Added, Removed };
    Kind kind;
    std::string text;
};

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) lines.push_back(line);
    return lines;
}

constexpr size_t kMaxDiffCells = 4'000'000;

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

}

void DrawPendingEditsPanel(PluginHost& plugin_host) {
    std::vector<PendingEdit> edits = plugin_host.PendingEdits();
    if (edits.empty()) return;

    char title[64];
    std::snprintf(title, sizeof(title), "Cambios propuestos (%zu)###pending_edits", edits.size());

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

    if (!open) {
        for (const PendingEdit& edit : edits) plugin_host.RejectEdit(edit.id);
    }
}

}

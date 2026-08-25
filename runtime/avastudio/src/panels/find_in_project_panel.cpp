#include "panels/find_in_project_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"
#include "util/project_utils.h"

namespace studio {

namespace fs = std::filesystem;

namespace {

// Same shape as ProblemsState's TrFormat (problems_panel.cpp) -- kept local
// rather than shared because it's a two-line helper and every panel in this
// codebase that needs "%s" substitution already carries its own copy
// (editor_panel.cpp, problems_panel.cpp) instead of a shared util for it.
std::string TrFormat(const std::string& key, std::initializer_list<std::string> args) {
    std::string result = util::Tr(key);
    for (const std::string& arg : args) {
        const size_t pos = result.find("%s");
        if (pos == std::string::npos) break;
        result = result.substr(0, pos) + arg + result.substr(pos + 2);
    }
    return result;
}

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

constexpr int kMaxMatches = 500;

}

void RunFindInProject(FindInProjectState& state, const std::string& project_root) {
    state.matches.clear();
    state.searched = true;
    state.files_scanned = 0;
    state.truncated = false;
    state.selected_index = -1;

    if (state.query.empty() || project_root.empty()) return;

    std::error_code ec;
    const fs::path root(project_root);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;

    // Fase 4: shared with Quick Open, extracted to project_utils.cpp so
    // neither reimplements its own file walk (ListSearchableFiles, formerly
    // this file's private CollectSearchableFiles).
    const std::vector<fs::path> files = ListSearchableFiles(root);

    const std::string needle = state.case_sensitive ? state.query : ToLower(state.query);
    const size_t needle_len = needle.size();

    for (const fs::path& file : files) {
        if (state.truncated) break;
        ++state.files_scanned;

        std::ifstream in(file, std::ios::binary);
        if (!in) continue;

        const std::string rel = fs::relative(file, root, ec).generic_string();
        const std::string display_path = ec ? file.generic_string() : rel;

        std::string line_text;
        int line_number = 0;
        while (std::getline(in, line_text)) {
            ++line_number;
            if (!line_text.empty() && line_text.back() == '\r') line_text.pop_back();

            const std::string haystack = state.case_sensitive ? line_text : ToLower(line_text);

            size_t pos = 0;
            while ((pos = haystack.find(needle, pos)) != std::string::npos) {
                FindInProjectMatch match;
                match.file = display_path;
                match.line = line_number;
                match.column_start = static_cast<int>(pos) + 1;
                match.column_end = static_cast<int>(pos + needle_len) + 1;
                match.line_text = line_text;
                state.matches.push_back(std::move(match));

                pos += needle_len;

                if (static_cast<int>(state.matches.size()) >= kMaxMatches) {
                    state.truncated = true;
                    break;
                }
            }
            if (state.truncated) break;
        }
    }
}

namespace {

std::string TrimForDisplay(const std::string& text) {
    size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    return text.substr(start);
}

}

std::optional<FindInProjectClickRequest> DrawFindInProjectPanel(FindInProjectState& state,
                                                                 const std::string& project_root, bool* p_open) {
    std::optional<FindInProjectClickRequest> click_request;

    const std::string title = util::Tr("panel.find_in_project.title") + "###find_in_project";
    ImGui::Begin(title.c_str(), p_open);

    ImGui::TextDisabled("%s", util::Tr("find_in_project.query_label").c_str());
    ImGui::SameLine();

    if (state.focus_query_field) {
        ImGui::SetKeyboardFocusHere();
        state.focus_query_field = false;
    }

    ImGui::SetNextItemWidth(-1.0f);
    const bool submitted = ImGui::InputText("##find_in_project_query", &state.query,
                                             ImGuiInputTextFlags_EnterReturnsTrue);

    const bool case_sensitive_changed = ImGui::Checkbox(util::Tr("find_in_project.case_sensitive").c_str(),
                                                          &state.case_sensitive);
    ImGui::SameLine();
    const bool search_clicked = ImGui::Button(util::Tr("find_in_project.search_button").c_str());

    // Both submitted-in-field and the explicit button trigger a (re)search;
    // toggling case-sensitivity re-runs the last query too, same
    // "don't make the user re-type to see the effect" reasoning the Settings
    // panel already applies elsewhere in this codebase for other toggles.
    if ((submitted || search_clicked || (case_sensitive_changed && state.searched)) && !state.query.empty()) {
        RunFindInProject(state, project_root);
    }

    ImGui::Separator();

    ImGui::BeginChild("find_in_project_results", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (!state.searched) {
        ImGui::TextDisabled("%s", util::Tr("find_in_project.empty_hint").c_str());
    } else if (state.matches.empty()) {
        ImGui::TextDisabled("%s", util::Tr("find_in_project.no_results").c_str());
    } else {
        const std::string summary = state.truncated
                                         ? TrFormat("find_in_project.results_truncated",
                                                    {std::to_string(state.matches.size())})
                                         : TrFormat("find_in_project.results_count",
                                                    {std::to_string(state.matches.size())});
        ImGui::TextDisabled("%s", summary.c_str());
        ImGui::Separator();

        std::string current_file;
        for (int i = 0; i < static_cast<int>(state.matches.size()); ++i) {
            const FindInProjectMatch& match = state.matches[static_cast<size_t>(i)];
            if (match.file != current_file) {
                current_file = match.file;
                ImGui::TextColored(palette::FromHex(palette::kAccentGold), "%s", current_file.c_str());
            }

            ImGui::PushID(i);
            const std::string label = "  " + std::to_string(match.line) + ": " + TrimForDisplay(match.line_text);
            if (ImGui::Selectable(label.c_str(), state.selected_index == i)) {
                state.selected_index = i;
                click_request = FindInProjectClickRequest{match.file, match.line, match.column_start,
                                                            match.column_end};
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::End();

    return click_request;
}

}

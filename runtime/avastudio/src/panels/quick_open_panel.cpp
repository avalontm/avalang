#include "panels/quick_open_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"
#include "util/project_utils.h"

namespace studio {

namespace fs = std::filesystem;

namespace {

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Same plain-substring scope as RunFindInProject/Command Palette -- no
// fuzzy/scored matching, deliberately (mirrors the "sin fuzzy scoring"
// scope note carried over from Fase 2.5/Fase 3).
bool Matches(const std::string& display_lower, const std::string& query_lower) {
    if (query_lower.empty()) return true;
    return display_lower.find(query_lower) != std::string::npos;
}

constexpr const char* kPopupId = "Quick Open##QuickOpen";

}

void OpenQuickOpen(QuickOpenState& state, const std::string& project_root) {
    state.query.clear();
    state.selected_index = 0;
    state.focus_query_field = true;

    state.files.clear();
    const fs::path root(project_root);
    for (const fs::path& file : ListSearchableFiles(root)) {
        std::error_code ec;
        const std::string rel = fs::relative(file, root, ec).generic_string();

        QuickOpenState::Entry entry;
        entry.display = ec ? file.generic_string() : rel;
        // Absolute (or root-relative-to-cwd, same as Explorer's path_str),
        // never the display path -- OpenFileInTab reads straight off
        // std::ifstream(path), which needs a path that resolves without
        // knowing project_root, same reasoning Explorer already follows for
        // its own file_to_open.
        entry.full_path = file.generic_string();
        state.files.push_back(std::move(entry));
    }

    ImGui::OpenPopup(kPopupId);
}

std::optional<std::string> DrawQuickOpen(QuickOpenState& state) {
    std::optional<std::string> picked;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + viewport->WorkSize.y * 0.28f),
                             ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    const bool open = ImGui::BeginPopupModal(
        kPopupId, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    if (!open) {
        ImGui::PopStyleVar();
        return picked;
    }

    if (state.focus_query_field) {
        ImGui::SetKeyboardFocusHere();
        state.focus_query_field = false;
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##quick_open_query", util::Tr("quick_open.hint").c_str(), &state.query);

    ImGui::Separator();

    const std::string query_lower = ToLower(state.query);
    std::vector<int> filtered;
    filtered.reserve(state.files.size());
    for (int i = 0; i < static_cast<int>(state.files.size()); ++i) {
        if (Matches(ToLower(state.files[static_cast<size_t>(i)].display), query_lower)) {
            filtered.push_back(i);
        }
    }

    if (filtered.empty()) {
        state.selected_index = 0;
    } else if (state.selected_index >= static_cast<int>(filtered.size())) {
        state.selected_index = static_cast<int>(filtered.size()) - 1;
    } else if (state.selected_index < 0) {
        state.selected_index = 0;
    }

    if (!filtered.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            state.selected_index = std::min(state.selected_index + 1, static_cast<int>(filtered.size()) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            state.selected_index = std::max(state.selected_index - 1, 0);
        }
    }

    const bool pick_selected =
        !filtered.empty() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
    const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape);

    ImGui::BeginChild("quick_open_results", ImVec2(0.0f, 320.0f), false);
    if (state.files.empty()) {
        ImGui::TextDisabled("%s", util::Tr("quick_open.no_project_files").c_str());
    } else if (filtered.empty()) {
        ImGui::TextDisabled("%s", util::Tr("quick_open.no_results").c_str());
    } else {
        for (int row = 0; row < static_cast<int>(filtered.size()); ++row) {
            const QuickOpenState::Entry& entry =
                state.files[static_cast<size_t>(filtered[static_cast<size_t>(row)])];
            ImGui::PushID(row);

            const bool is_selected = row == state.selected_index;
            if (is_selected) {
                ImGui::SetScrollHereY(0.35f);
            }
            if (ImGui::Selectable(entry.display.c_str(), is_selected, ImGuiSelectableFlags_None)) {
                state.selected_index = row;
                picked = entry.full_path;
            }
            ImGui::PopID();
        }

        if (pick_selected) {
            picked = state.files[static_cast<size_t>(filtered[static_cast<size_t>(state.selected_index)])].full_path;
        }
    }
    ImGui::EndChild();

    // Same CloseCurrentPopup()-before-EndPopup()-but-return-after ordering
    // Command Palette already uses -- picking a file doesn't open another
    // popup today, but keeping the same shape means it wouldn't collide if
    // a future caller reacted to the pick by opening one.
    if (picked || cancel) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar();

    return picked;
}

}

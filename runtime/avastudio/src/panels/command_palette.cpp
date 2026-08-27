#include "panels/command_palette.h"

#include <algorithm>
#include <cctype>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Same plain-substring scope as RunFindInProject (find_in_project_panel.cpp)
// -- no fuzzy/scored matching, deliberately, per the Fase 3 design (§5.2's
// scope note carried over: "sin fuzzy scoring").
bool Matches(const std::string& label_lower, const std::string& query_lower) {
    if (query_lower.empty()) return true;
    return label_lower.find(query_lower) != std::string::npos;
}

constexpr const char* kPopupId = "Command Palette##CommandPalette";

}

void OpenCommandPalette(CommandPaletteState& state) {
    state.query.clear();
    state.selected_index = 0;
    state.focus_query_field = true;
    ImGui::OpenPopup(kPopupId);
}

void DrawCommandPalette(CommandPaletteState& state, const std::vector<Command>& commands) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + viewport->WorkSize.y * 0.28f),
                             ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    const bool open = ImGui::BeginPopupModal(
        kPopupId, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    if (!open) {
        ImGui::PopStyleVar();
        return;
    }

    // Captured before focus_query_field gets consumed/cleared below -- true
    // only on the single frame the palette just opened (OpenCommandPalette
    // arms it), used further down to decide whether the selected row should
    // force-scroll into view this frame.
    const bool just_opened = state.focus_query_field;

    if (state.focus_query_field) {
        ImGui::SetKeyboardFocusHere();
        state.focus_query_field = false;
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##command_palette_query", util::Tr("command_palette.hint").c_str(), &state.query);

    ImGui::Separator();

    const std::string query_lower = ToLower(state.query);
    std::vector<int> filtered;
    filtered.reserve(commands.size());
    for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
        if (Matches(ToLower(commands[static_cast<size_t>(i)].label), query_lower)) {
            filtered.push_back(i);
        }
    }

    // Snapshot before clamping/arrow-key handling below, so we can tell
    // afterwards whether the selection actually moved this frame (as
    // opposed to just staying on the same row while the mouse scrolls the
    // list manually -- see selection_moved / SetScrollHereY below).
    const int index_at_frame_start = state.selected_index;

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

    // Only force-scroll to the selected row on frames where the selection
    // actually changed (arrow keys, the filtered list reshuffling under
    // typing, or the palette just opening) -- NOT on every single frame the
    // row happens to still be selected. Calling SetScrollHereY every frame
    // regardless (the previous behavior) fights any manual scroll: as soon
    // as the user scrolls the list with the mouse wheel or drags the
    // scrollbar, the very next frame snaps back to the selected row (which
    // sits near the top right after opening), making the list feel stuck
    // and impossible to scroll down through.
    const bool selection_moved = just_opened || (state.selected_index != index_at_frame_start);

    const bool run_selected =
        !filtered.empty() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
    const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape);

    std::function<void()> action_to_run;

    ImGui::BeginChild("command_palette_results", ImVec2(0.0f, 320.0f), false);
    if (filtered.empty()) {
        ImGui::TextDisabled("%s", util::Tr("command_palette.no_results").c_str());
    } else {
        for (int row = 0; row < static_cast<int>(filtered.size()); ++row) {
            const Command& cmd = commands[static_cast<size_t>(filtered[static_cast<size_t>(row)])];
            ImGui::PushID(row);

            const bool is_selected = row == state.selected_index;
            if (is_selected && selection_moved) {
                ImGui::SetScrollHereY(0.35f);
            }
            if (ImGui::Selectable(cmd.label.c_str(), is_selected, ImGuiSelectableFlags_None)) {
                state.selected_index = row;
                action_to_run = cmd.action;
            }
            if (!cmd.shortcut.empty()) {
                const ImVec2 shortcut_size = ImGui::CalcTextSize(cmd.shortcut.c_str());
                const float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, avail - shortcut_size.x));
                ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", cmd.shortcut.c_str());
            }

            ImGui::PopID();
        }

        if (run_selected) {
            action_to_run = commands[static_cast<size_t>(filtered[static_cast<size_t>(state.selected_index)])].action;
        }
    }
    ImGui::EndChild();

    // CloseCurrentPopup() has to be called while this popup is still the
    // active one on ImGui's popup stack, i.e. before EndPopup() -- unlike
    // running the action itself, which is deferred until after EndPopup()
    // below so a command that opens another popup/modal this same frame
    // (none currently do, but future commands might) doesn't collide with
    // the Command Palette's own still-open stack entry.
    if (action_to_run || cancel) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar();

    if (action_to_run) {
        action_to_run();
    }
}

}

#include "panels/debug_panel.h"

#include <utility>
#include <vector>

#include "imgui.h"
#include "palette.h"
#include "util/i18n.h"

namespace studio {

namespace {

const char* PhaseTrKey(debug::DebugPhase phase) {
    switch (phase) {
        case debug::DebugPhase::Idle: return "debug.phase.idle";
        case debug::DebugPhase::Starting: return "debug.phase.starting";
        case debug::DebugPhase::Running: return "debug.phase.running";
        case debug::DebugPhase::Paused: return "debug.phase.paused";
        case debug::DebugPhase::Ended: return "debug.phase.ended";
    }
    return "debug.phase.idle";
}

unsigned int PhaseColor(debug::DebugPhase phase) {
    switch (phase) {
        case debug::DebugPhase::Idle: return palette::kTextMuted;
        case debug::DebugPhase::Starting: return palette::kWarning;
        case debug::DebugPhase::Running: return palette::kSuccess;
        case debug::DebugPhase::Paused: return palette::kInfo;
        case debug::DebugPhase::Ended: return palette::kTextMuted;
    }
    return palette::kTextMuted;
}

void DrawToolbar(debug::DebugSession& session, DebugPanelResult& result) {
    const debug::DebugPhase phase = session.Phase();
    const bool paused = phase == debug::DebugPhase::Paused;
    const bool active = session.IsActive();

    if (!active) {
        if (ImGui::Button(util::Tr("debug.start_button").c_str())) result.start_requested = true;
    } else {
        ImGui::BeginDisabled(!paused);
        if (ImGui::Button(util::Tr("debug.continue_button").c_str())) session.Continue();
        ImGui::SameLine();
        if (ImGui::Button(util::Tr("debug.step_over_button").c_str())) session.StepOver();
        ImGui::SameLine();
        if (ImGui::Button(util::Tr("debug.step_into_button").c_str())) session.StepInto();
        ImGui::SameLine();
        if (ImGui::Button(util::Tr("debug.step_out_button").c_str())) session.StepOut();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(phase != debug::DebugPhase::Running);
        if (ImGui::Button(util::Tr("debug.pause_button").c_str())) session.Pause();
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(util::Tr("debug.stop_button").c_str())) session.Stop();
    }

    ImGui::SameLine();
    ImGui::TextColored(palette::FromHex(PhaseColor(phase)), "%s", util::Tr(PhaseTrKey(phase)).c_str());

    if (!session.Error().empty()) {
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", session.Error().c_str());
    } else if (paused && !session.StopReason().empty()) {
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                            util::TrFormat("debug.stopped_reason", {session.StopReason()}).c_str());
    }
}

std::optional<ProblemsFileClickRequest> DrawCallStack(debug::DebugSession& session) {
    std::optional<ProblemsFileClickRequest> click;

    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("debug.section_call_stack").c_str());
    ImGui::BeginChild("debug_call_stack", ImVec2(0, 120), true);

    const auto& frames = session.Frames();
    for (size_t i = 0; i < frames.size(); ++i) {
        const debug::DebugFrame& frame = frames[i];
        ImGui::PushID(static_cast<int>(i));
        const std::string label = frame.name + "  " + frame.file + ":" + std::to_string(frame.line);
        if (ImGui::Selectable(label.c_str(), i == session.SelectedFrame())) {
            session.SelectFrame(i);
            click = ProblemsFileClickRequest{frame.file, frame.line, frame.column, frame.name};
        }
        ImGui::PopID();
    }
    if (frames.empty()) ImGui::TextDisabled("%s", util::Tr("debug.no_call_stack").c_str());

    ImGui::EndChild();
    return click;
}

void DrawVariables(debug::DebugSession& session) {
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("debug.section_variables").c_str());
    ImGui::BeginChild("debug_variables", ImVec2(0, 120), true);

    const auto& variables = session.Variables();
    for (const debug::DebugVariable& variable : variables) {
        ImGui::TextColored(palette::FromHex(palette::kSynVariable), "%s", variable.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", variable.type.c_str());
        ImGui::SameLine();
        ImGui::Text("= %s", variable.value.c_str());
    }
    if (variables.empty()) ImGui::TextDisabled("%s", util::Tr("debug.no_variables").c_str());

    ImGui::EndChild();
}

std::optional<ProblemsFileClickRequest> DrawBreakpoints(debug::DebugSession& session) {
    std::optional<ProblemsFileClickRequest> click;

    const float row_right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("debug.section_breakpoints").c_str());
    ImGui::SameLine();
    const std::string clear_label = util::Tr("debug.clear_breakpoints_button");
    const float clear_w = ImGui::CalcTextSize(clear_label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float clear_x = row_right_x - clear_w;
    if (ImGui::GetCursorPosX() < clear_x) ImGui::SetCursorPosX(clear_x);
    if (ImGui::SmallButton(clear_label.c_str())) session.ClearBreakpoints();

    ImGui::BeginChild("debug_breakpoints", ImVec2(0, 0), true);

    std::vector<std::pair<std::string, int>> flat;
    for (const auto& entry : session.Breakpoints()) {
        for (int line : entry.second) flat.emplace_back(entry.first, line);
    }

    for (const auto& [file, line] : flat) {
        ImGui::PushID((file + ":" + std::to_string(line)).c_str());
        if (ImGui::SmallButton("x")) session.ToggleBreakpoint(file, line);
        ImGui::SameLine();
        const std::string label = file + ":" + std::to_string(line);
        if (ImGui::Selectable(label.c_str())) click = ProblemsFileClickRequest{file, line, 0, ""};
        ImGui::PopID();
    }
    if (flat.empty()) ImGui::TextDisabled("%s", util::Tr("debug.no_breakpoints").c_str());

    ImGui::EndChild();
    return click;
}

void DrawLog(debug::DebugSession& session) {
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("debug.section_log").c_str());
    ImGui::BeginChild("debug_log", ImVec2(0, 100), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(session.Log().c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

}  // namespace

DebugPanelResult DrawDebugPanel(debug::DebugSession& session, bool* p_open) {
    DebugPanelResult result;

    const std::string title = util::Tr("panel.debug.title") + "###debug";
    ImGui::Begin(title.c_str(), p_open);

    DrawToolbar(session, result);
    ImGui::Separator();

    if (auto click = DrawCallStack(session)) result.file_click = click;
    DrawVariables(session);
    if (auto click = DrawBreakpoints(session); click && !result.file_click) result.file_click = click;
    DrawLog(session);

    ImGui::End();
    return result;
}

}  // namespace studio

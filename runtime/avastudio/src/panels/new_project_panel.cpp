#include "panels/new_project_panel.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"
#include "util/scaffold_templates.h"

namespace studio {

namespace fs = std::filesystem;

namespace {

constexpr const char* kPopupId = "New Project##NewProject";

// Same TrFormat(key, arg) shape Explorer/Editor/Canvas/Build already carry
// as their own file-local helper (never shared between panels, per those
// phases' own reasoning) -- substitutes the value's *already-translated*
// "%s" placeholder before handing it to ImGui.
std::string TrFormat(const std::string& key, const std::string& arg) {
    const std::string& fmt = util::Tr(key);
    const size_t pos = fmt.find("%s");
    if (pos == std::string::npos) return fmt;
    return fmt.substr(0, pos) + arg + fmt.substr(pos + 2);
}

// Fixed entry-script boilerplate for a brand new project. Deliberately NOT
// routed through data/scaffold/file_templates.csv the way "Generar" (Fase
// 5) does for class/screen files -- scaffold_templates.h's own comment
// already carves the project wizard out as "a separate flow", and there's
// only ever one variant here (no {ClassName}/{DisplayName} substitution
// needed), so a CSV row would just be one more place this string could
// drift from what main.ava actually gets. Same top-level-script shape
// samples/test/main.ava already uses -- a new project's entry point isn't
// a class, so Fase 5's kClass template (constructor boilerplate) doesn't
// fit here either.
const char* kMainAvaContent =
    "import system\n"
    "\n"
    "print(\"Hello, AvaLang!\")\n";

}  // namespace

void OpenNewProjectDialog(NewProjectState& state, const std::string& default_destination) {
    state.name.clear();
    state.destination = default_destination;
    state.template_kind = NewProjectTemplateKind::kEmpty;
    state.error_key.clear();
    state.focus_name_field = true;
    ImGui::OpenPopup(kPopupId);
}

NewProjectDrawResult DrawNewProjectDialog(NewProjectState& state) {
    NewProjectDrawResult result;

    // Same centered-near-the-top placement and fixed width Command
    // Palette/Quick Open already use for their own popups -- keeps every
    // modal overlay in this app appearing in the same spot instead of each
    // one picking its own.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + viewport->WorkSize.y * 0.28f),
                             ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));

    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        return result;
    }

    if (state.focus_name_field) {
        ImGui::SetKeyboardFocusHere();
        state.focus_name_field = false;
    }
    ImGui::TextUnformatted(util::Tr("new_project.name_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    const bool enter_in_name = ImGui::InputTextWithHint("##new_project_name", util::Tr("new_project.name_hint").c_str(),
                                                          &state.name, ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();
    ImGui::TextUnformatted(util::Tr("new_project.destination_label").c_str());
    ImGui::SetNextItemWidth(-70.0f);
    ImGui::InputText("##new_project_destination", &state.destination);
    ImGui::SameLine();
    if (ImGui::Button(util::Tr("common.browse").c_str(), ImVec2(60.0f, 0.0f))) {
        result.browse_destination_requested = true;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(util::Tr("new_project.template_label").c_str());
    // Two radios, not a combo -- same "always-visible binary choice"
    // reasoning already applied to Explorer's own Class/Screen radios
    // (Fase 5) and Build's Desktop/BareKernel combo-turned-checkbox family.
    const bool is_empty = state.template_kind == NewProjectTemplateKind::kEmpty;
    if (ImGui::RadioButton(util::Tr("new_project.template_empty").c_str(), is_empty)) {
        state.template_kind = NewProjectTemplateKind::kEmpty;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(util::Tr("new_project.template_with_screen").c_str(), !is_empty)) {
        state.template_kind = NewProjectTemplateKind::kWithScreen;
    }

    // Visibility of system state (§5's own usability criteria list): show
    // exactly the folder that's about to be created before the user
    // commits, instead of only finding out from the error message if
    // something about the combined path was wrong.
    if (!state.name.empty() && !state.destination.empty()) {
        const fs::path preview = fs::path(state.destination) / state.name;
        ImGui::Spacing();
        ImGui::TextDisabled("%s", TrFormat("new_project.preview_label", preview.string()).c_str());
    }

    if (!state.error_key.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", util::Tr(state.error_key).c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool create_clicked = ImGui::Button(util::Tr("new_project.create_button").c_str());
    ImGui::SameLine();
    const bool cancel_clicked = ImGui::Button(util::Tr("common.cancel").c_str());

    if (create_clicked || enter_in_name) {
        state.error_key.clear();
        if (state.name.empty()) {
            state.error_key = "new_project.error_name_required";
        } else if (state.destination.empty()) {
            state.error_key = "new_project.error_destination_required";
        } else {
            std::error_code ec;
            const fs::path destination_path(state.destination);
            if (!fs::is_directory(destination_path, ec)) {
                state.error_key = "new_project.error_destination_missing";
            } else {
                const fs::path project_dir = destination_path / state.name;
                if (fs::exists(project_dir, ec)) {
                    // Never write into a folder that's already there --
                    // same "don't silently merge into something unrelated"
                    // reasoning behind the delete-confirmation modal
                    // Explorer already has, just applied before creating
                    // instead of before destroying.
                    state.error_key = "new_project.error_already_exists";
                } else {
                    fs::create_directories(project_dir, ec);
                    if (ec) {
                        state.error_key = "new_project.error_create_failed";
                    } else {
                        const fs::path main_ava = project_dir / "main.ava";
                        std::ofstream out(main_ava.string(), std::ios::binary);
                        out << kMainAvaContent;
                        out.close();

                        if (state.template_kind == NewProjectTemplateKind::kWithScreen) {
                            const fs::path screen_avaui = project_dir / "screen.avaui";
                            std::ofstream screen_out(screen_avaui.string(), std::ios::binary);
                            screen_out << util::BuildScaffoldContent(util::ScaffoldKind::kScreen, "Home");
                            screen_out.close();
                        }

                        NewProjectResult created;
                        created.project_dir = project_dir.string();
                        created.entry_file = main_ava.string();
                        result.created = created;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
    }

    if (cancel_clicked) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    return result;
}

}

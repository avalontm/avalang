#include "panels/new_project_panel.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "project/avaproj_file.h"
#include "project/project_config.h"
#include "util/i18n.h"
#include "util/ui_widgets.h"
#include "util/scaffold_templates.h"

namespace studio {

namespace fs = std::filesystem;

namespace {

constexpr const char* kPopupId = "New Project##NewProject";

const char* kMainAvaContentConsole =
    "import system\n"
    "\n"
    "print(\"Hello, AvaLang!\")\n";

// UI template's main.ava: kept as plain console boilerplate on purpose --
// there's no native call yet (e.g. a Window.Show-style API) that opens an
// .avaui screen from a running script. Screens under views/ are edited and
// previewed from AvaStudio's Preview panel, not launched from here.
const char* kMainAvaContentUi =
    "import system\n"
    "\n"
    "# Las pantallas de este proyecto viven en views/ (ver views/Home.avaui).\n"
    "# Se editan y se previsualizan desde el panel Preview de AvaStudio.\n"
    "\n"
    "print(\"Hello, AvaLang!\")\n";

const char* kAppAvaContentUi =
    "style \"styles.ava\"\n";

// Small filled-square icon, same visual language as Explorer's file-kind
// list (explorer_panel.cpp::DrawIcon) -- kept as its own copy here since
// it's a 6-line local helper, not worth sharing across translation units.
void DrawIcon(ImU32 color) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight();
    const float s = h * 0.55f;
    const float y = p.y + (h - s) * 0.5f;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + s, y + s), color, 2.0f);
    ImGui::Dummy(ImVec2(s, h));
    ImGui::SameLine(0.0f, 6.0f);
}

// One entry in the "will create" preview tree below the template cards.
struct PreviewEntry {
    const char* path;    // relative to the project folder; trailing '/' = folder row
    unsigned int color;  // icon color -- matches Explorer's file-kind colors
};

constexpr PreviewEntry kConsolePreview[] = {
    {"main.ava", palette::kPrimary},
};

constexpr PreviewEntry kUiPreview[] = {
    {"main.ava", palette::kPrimary},
    {"app.ava", palette::kPrimary},
    {"styles.ava", palette::kPrimary},
    {"views/", palette::kSynType},
    {"views/Home.avaui", palette::kAccentGold},
};

// Draws one selectable template card: an icon + title inside a bordered
// child, with a short description wrapped below it. Returns true if the
// user clicked the card this frame. Implemented with BeginChild + a
// following IsItemClicked/IsItemHovered check (the child window itself
// acts as a single hoverable/clickable item in the parent window) rather
// than a plain RadioButton, so the whole card -- icon, title and
// description -- is one click target instead of just a small dot + label.
bool DrawTemplateCard(const char* id, bool selected, ImU32 icon_color, const std::string& title,
                      const std::string& description, const ImVec2& size) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? palette::FromHex(palette::kPrimary)
                                                     : palette::FromHex(palette::kBorder));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, selected ? palette::FromHex(palette::kPrimary, 0.12f)
                                                      : palette::FromHex(palette::kCard));
    ImGui::BeginChild("##card", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    DrawIcon(icon_color);
    ImGui::TextUnformatted(title.c_str());
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + size.x - ImGui::GetStyle().WindowPadding.x * 2.0f);
    ImGui::TextDisabled("%s", description.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopID();

    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

}  // namespace

void OpenNewProjectDialog(NewProjectState& state, const std::string& default_destination) {
    state.name.clear();
    state.destination = default_destination;
    state.template_kind = NewProjectTemplateKind::kConsole;
    state.error_key.clear();
    state.focus_name_field = true;
    ImGui::OpenPopup(kPopupId);
}

NewProjectDrawResult DrawNewProjectDialog(NewProjectState& state) {
    NewProjectDrawResult result;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + viewport->WorkSize.y * 0.24f),
                             ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));

    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        return result;
    }

    // Contextual errors are checked once here and shown right under the
    // field they belong to, instead of one generic sentence at the bottom
    // -- so the user doesn't have to guess which field a message refers to.
    const bool name_error = state.error_key == "new_project.error_name_required";
    const bool destination_error = state.error_key == "new_project.error_destination_required" ||
                                    state.error_key == "new_project.error_destination_missing";
    const bool creation_error = state.error_key == "new_project.error_already_exists" ||
                                 state.error_key == "new_project.error_create_failed";

    ImGui::TextDisabled("%s", util::Tr("new_project.section_location").c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (state.focus_name_field) {
        ImGui::SetKeyboardFocusHere();
        state.focus_name_field = false;
    }
    ImGui::TextUnformatted(util::Tr("new_project.name_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (name_error) ImGui::PushStyleColor(ImGuiCol_Border, palette::FromHex(palette::kError));
    const bool enter_in_name = ImGui::InputTextWithHint("##new_project_name", util::Tr("new_project.name_hint").c_str(),
                                                          &state.name, ImGuiInputTextFlags_EnterReturnsTrue);
    if (name_error) {
        ImGui::PopStyleColor();
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", util::Tr(state.error_key).c_str());
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(util::Tr("new_project.destination_label").c_str());
    ImGui::SetNextItemWidth(-70.0f);
    if (destination_error) ImGui::PushStyleColor(ImGuiCol_Border, palette::FromHex(palette::kError));
    ImGui::InputText("##new_project_destination", &state.destination);
    if (destination_error) ImGui::PopStyleColor();
    ImGui::SameLine();
    const std::string browse_label = util::Tr("common.browse");
    if (ImGui::Button(browse_label.c_str(), util::AutoButtonSize(browse_label.c_str(), 60.0f))) {
        result.browse_destination_requested = true;
    }
    if (destination_error) {
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", util::Tr(state.error_key).c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", util::Tr("new_project.section_template").c_str());
    ImGui::Separator();
    ImGui::Spacing();

    const bool is_console = state.template_kind == NewProjectTemplateKind::kConsole;
    const float card_gap = ImGui::GetStyle().ItemSpacing.x;
    const float card_w = (ImGui::GetContentRegionAvail().x - card_gap) * 0.5f;
    const ImVec2 card_size(card_w, 84.0f);

    if (DrawTemplateCard("console", is_console, palette::U32FromHex(palette::kPrimary),
                          util::Tr("new_project.template_console"), util::Tr("new_project.template_console_desc"),
                          card_size)) {
        state.template_kind = NewProjectTemplateKind::kConsole;
    }
    ImGui::SameLine();
    if (DrawTemplateCard("ui", !is_console, palette::U32FromHex(palette::kAccentGold),
                          util::Tr("new_project.template_ui"), util::Tr("new_project.template_ui_desc"),
                          card_size)) {
        state.template_kind = NewProjectTemplateKind::kUi;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", util::Tr("new_project.preview_header").c_str());
    ImGui::Spacing();

    // "Will create" tree -- shows exactly which files/folders the click on
    // Create is about to write, so the user isn't guessing what a template
    // contains before committing to it. Falls back to just the project
    // name if the name/destination fields aren't filled in yet.
    {
        const std::string project_name = state.name.empty() ? util::Tr("new_project.name_hint") : state.name;
        DrawIcon(palette::U32FromHex(palette::kSynType));
        ImGui::TextUnformatted((project_name + "/").c_str());

        const bool is_ui = state.template_kind == NewProjectTemplateKind::kUi;
        const PreviewEntry* entries = is_ui ? kUiPreview : kConsolePreview;
        const size_t count = is_ui ? std::size(kUiPreview) : std::size(kConsolePreview);
        for (size_t i = 0; i < count; ++i) {
            ImGui::Indent();
            DrawIcon(palette::U32FromHex(entries[i].color));
            ImGui::TextUnformatted(entries[i].path);
            ImGui::Unindent();
        }
    }

    if (creation_error) {
        ImGui::Spacing();
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", util::Tr(state.error_key).c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool can_submit = !state.name.empty() && !state.destination.empty();
    ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kPrimary));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kPrimaryHover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(palette::kPrimaryDark));
    ImGui::BeginDisabled(!can_submit);
    const bool create_clicked = ImGui::Button(util::Tr("new_project.create_button").c_str());
    ImGui::EndDisabled();
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    const bool cancel_clicked = ImGui::Button(util::Tr("common.cancel").c_str());

    if ((create_clicked || (enter_in_name && can_submit))) {
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
                    state.error_key = "new_project.error_already_exists";
                } else {
                    fs::create_directories(project_dir, ec);
                    if (ec) {
                        state.error_key = "new_project.error_create_failed";
                    } else {
                        const bool is_ui = state.template_kind == NewProjectTemplateKind::kUi;

                        const fs::path main_ava = project_dir / "main.ava";
                        std::ofstream out(main_ava.string(), std::ios::binary);
                        out << (is_ui ? kMainAvaContentUi : kMainAvaContentConsole);
                        out.close();

                        if (is_ui) {
                            const fs::path app_ava = project_dir / "app.ava";
                            std::ofstream app_out(app_ava.string(), std::ios::binary);
                            app_out << kAppAvaContentUi;
                            app_out.close();

                            // Vacio al crear -- el usuario agrega sus reglas
                            // a medida que las necesita, misma convencion que
                            // ya usa AvaHost (samples/web/testproj/styles.ava).
                            const fs::path styles_ava = project_dir / "styles.ava";
                            std::ofstream(styles_ava.string(), std::ios::binary).close();

                            const fs::path views_dir = project_dir / "views";
                            fs::create_directories(views_dir, ec);

                            const fs::path home_avaui = views_dir / "Home.avaui";
                            std::ofstream home_out(home_avaui.string(), std::ios::binary);
                            home_out << util::BuildScaffoldContent(util::ScaffoldKind::kScreen, "Home");
                            home_out.close();
                        }

                        AvaProjFile avaproj;
                        avaproj.project_name = state.name;
                        avaproj.entry_file = "main.ava";
                        avaproj.out_dir = "bin";
                        SaveAvaProjFile((project_dir / (state.name + ".avaproj")).string(), avaproj);
                        EnsureGitignoreEntry(project_dir.string(), "*.avaproj.user");

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
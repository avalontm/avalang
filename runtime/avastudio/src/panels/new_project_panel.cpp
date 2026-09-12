#include "panels/new_project_panel.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

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

// Data-driven template catalog, Visual Studio's "Create a new project"
// list style: each entry carries its own icon, title, tag and description,
// so the browser below (search box + category filter + scrollable list)
// stays generic and adding a future template is just one more row here
// instead of a new hardcoded card/branch in the draw function.
struct ProjectTemplate {
    NewProjectTemplateKind kind;
    const char* id;
    unsigned int icon_color;
    const char* title_key;
    const char* tag_key;
    const char* desc_key;
};

constexpr ProjectTemplate kTemplates[] = {
    {NewProjectTemplateKind::kConsole, "console", palette::kPrimary, "new_project.template_console",
     "new_project.template_console", "new_project.template_console_desc"},
    {NewProjectTemplateKind::kUi, "ui", palette::kAccentGold, "new_project.template_ui", "new_project.template_ui",
     "new_project.template_ui_desc"},
};

std::string ToLower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool TemplateMatchesFilters(const ProjectTemplate& tpl, const std::string& search, const std::string& category) {
    if (!category.empty() && util::Tr(tpl.tag_key) != category) return false;
    if (search.empty()) return true;
    const std::string haystack = ToLower(util::Tr(tpl.title_key) + " " + util::Tr(tpl.desc_key));
    return haystack.find(ToLower(search)) != std::string::npos;
}

// Small pill-shaped label, same visual language as the editor's hint
// badges (editor_panel.cpp::DrawHintBadge) -- kept as its own copy here
// since it's a handful of lines, not worth sharing across translation
// units. Used for the category tag next to a template's title.
void DrawTag(const char* label) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 padding(6.0f, 2.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 box_max(origin.x + text_size.x + padding.x * 2.0f, origin.y + text_size.y + padding.y * 2.0f);
    draw_list->AddRectFilled(origin, box_max, palette::U32FromHex(palette::kBorder), 3.0f);
    draw_list->AddText(ImVec2(origin.x + padding.x, origin.y + padding.y), palette::U32FromHex(palette::kTextSecondary),
                        label);
    ImGui::Dummy(ImVec2(box_max.x - origin.x, box_max.y - origin.y));
}

// Small magnifying-glass glyph drawn on the draw list (the app's default
// font doesn't carry a search icon), placed to the left of the template
// search box the same way Visual Studio's template picker does. Sized and
// vertically centered against the input frame height (not the text line
// height) so it lines up with the box it sits next to instead of hugging
// the top of a taller frame.
void DrawSearchIcon(ImU32 color) {
    const float frame_h = ImGui::GetFrameHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float r = frame_h * 0.22f;
    const ImVec2 center(p.x + r + 2.0f, p.y + frame_h * 0.5f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircle(center, r, color, 0, 1.6f);
    const ImVec2 handle_start(center.x + r * 0.72f, center.y + r * 0.72f);
    draw_list->AddLine(handle_start, ImVec2(handle_start.x + r * 0.65f, handle_start.y + r * 0.65f), color, 1.6f);
    ImGui::Dummy(ImVec2(frame_h, frame_h));
    ImGui::SameLine(0.0f, 4.0f);
}

// Draws one row of the template list: icon + title + category tag on the
// first line, wrapped description below -- the same information density
// as a Visual Studio template row, minus the platform/language columns
// AvaLang doesn't have. The row itself is an InvisibleButton spanning the
// full width so the whole row (not just the title text) is one click
// target; content is drawn on top of it via BeginGroup/EndGroup after
// resetting the cursor back to the row's origin.
bool DrawTemplateRow(const ProjectTemplate& tpl, bool selected, float width) {
    ImGui::PushID(tpl.id);
    const float height = 56.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    const bool clicked = ImGui::InvisibleButton("##hit", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 row_max(origin.x + width, origin.y + height);
    if (selected) {
        draw_list->AddRectFilled(origin, row_max, palette::U32FromHex(palette::kPrimary, 0.14f), 4.0f);
        draw_list->AddRect(origin, row_max, palette::U32FromHex(palette::kPrimary), 4.0f, 0, 1.0f);
    } else if (hovered) {
        draw_list->AddRectFilled(origin, row_max, palette::U32FromHex(palette::kBorder, 0.5f), 4.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 10.0f, origin.y + 8.0f));
    ImGui::BeginGroup();
    DrawIcon(palette::U32FromHex(tpl.icon_color));
    ImGui::TextUnformatted(util::Tr(tpl.title_key).c_str());
    ImGui::SameLine();
    DrawTag(util::Tr(tpl.tag_key).c_str());

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
    ImGui::PushTextWrapPos(origin.x + width - 16.0f);
    ImGui::TextDisabled("%s", util::Tr(tpl.desc_key).c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::SetCursorScreenPos(ImVec2(origin.x, row_max.y));
    ImGui::PopID();

    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

}  // namespace

void OpenNewProjectDialog(NewProjectState& state, const std::string& default_destination) {
    state.name.clear();
    state.destination = default_destination;
    state.template_kind = NewProjectTemplateKind::kConsole;
    state.template_search.clear();
    state.template_category.clear();
    state.error_key.clear();
    state.focus_name_field = true;
    ImGui::OpenPopup(kPopupId);
}

NewProjectDrawResult DrawNewProjectDialog(NewProjectState& state) {
    NewProjectDrawResult result;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + viewport->WorkSize.y * 0.18f),
                             ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(680.0f, 0.0f));

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

    // Visual Studio-style template browser: a search box plus a category
    // combo filter a scrollable, data-driven list of rows (kTemplates)
    // instead of a fixed pair of side-by-side cards. Categories are
    // collected from the catalog itself so a future template only needs
    // a new entry in kTemplates, not a new UI branch here.
    ImGui::TextDisabled("%s", util::Tr("new_project.section_template").c_str());
    ImGui::Separator();
    ImGui::Spacing();

    std::vector<std::string> categories;
    categories.push_back(util::Tr("new_project.filter_all"));
    for (const ProjectTemplate& tpl : kTemplates) {
        std::string category = util::Tr(tpl.tag_key);
        if (std::find(categories.begin() + 1, categories.end(), category) == categories.end()) {
            categories.push_back(category);
        }
    }

    float category_w = 150.0f;
    for (const std::string& category : categories) {
        category_w = std::max(category_w, ImGui::CalcTextSize(category.c_str()).x + 40.0f);
    }

    const float icon_w = ImGui::GetFrameHeight() + 4.0f;
    const float total_w = ImGui::GetContentRegionAvail().x;
    const float search_w = total_w - icon_w - category_w - ImGui::GetStyle().ItemSpacing.x;

    DrawSearchIcon(palette::U32FromHex(palette::kTextMuted));
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputTextWithHint("##new_project_search", util::Tr("new_project.search_hint").c_str(),
                              &state.template_search);
    ImGui::SameLine();

    const std::string category_preview = state.template_category.empty() ? categories[0] : state.template_category;
    ImGui::SetNextItemWidth(category_w);
    if (ImGui::BeginCombo("##new_project_category", category_preview.c_str())) {
        for (size_t i = 0; i < categories.size(); ++i) {
            const bool is_all = i == 0;
            const bool selected = is_all ? state.template_category.empty() : categories[i] == state.template_category;
            if (ImGui::Selectable(categories[i].c_str(), selected)) {
                state.template_category = is_all ? std::string() : categories[i];
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, palette::FromHex(palette::kBorder));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette::FromHex(palette::kSurface));
    ImGui::BeginChild("##template_list", ImVec2(0.0f, 190.0f), true);

    bool any_visible = false;
    for (const ProjectTemplate& tpl : kTemplates) {
        if (!TemplateMatchesFilters(tpl, state.template_search, state.template_category)) continue;
        any_visible = true;
        if (DrawTemplateRow(tpl, state.template_kind == tpl.kind, ImGui::GetContentRegionAvail().x)) {
            state.template_kind = tpl.kind;
        }
    }
    if (!any_visible) {
        ImGui::TextDisabled("%s", util::Tr("new_project.no_results").c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();
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
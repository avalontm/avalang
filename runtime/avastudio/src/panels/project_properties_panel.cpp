#include "panels/project_properties_panel.h"

#include <filesystem>
#include <optional>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"
#include "util/project_utils.h"
#include "util/ui_widgets.h"

namespace studio {

namespace {
namespace fs = std::filesystem;

bool DrawPathRow(const char* label, const char* hint, std::string& value, const char* browse_id,
                  ProjectPropertiesBrowseField field, ProjectPropertiesResult& result) {
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputTextWithHint((std::string("##") + label).c_str(), hint, &value);
    bool committed = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();

    if (ImGui::Button((std::string(browse_id) + "##" + label).c_str(), ImVec2(80.0f, 0.0f))) {
        result.browse_requested = field;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    return committed;
}

void DrawApplicationTab(AvaProjFile& proj, const std::string& project_dir, ProjectPropertiesResult& result) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.name_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ProjectName", &proj.project_name);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.dirty = true;
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.entry_file_label").c_str());
    ImGui::SetNextItemWidth(-100.0f);
    ImGui::InputText("##ProjectEntryFile", &proj.entry_file);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.dirty = true;
    ImGui::SameLine();
    const std::string detect_label = util::Tr("build.detect_button");
    if (ImGui::Button(detect_label.c_str(), util::AutoButtonSize(detect_label.c_str(), 70.0f))) {
        std::error_code ec;
        if (fs::exists(project_dir, ec)) {
            proj.entry_file = DetectEntryFile(project_dir);
            result.dirty = true;
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.output_type_label").c_str());
    int type_index = proj.output_type == AvaProjOutputType::kBareKernel
                          ? 1
                          : (proj.output_type == AvaProjOutputType::kLibrary ? 2 : 0);
    const std::string type_exe = util::Tr("build.target_desktop");
    const std::string type_barekernel = util::Tr("build.target_barekernel");
    const std::string type_library = util::Tr("build.target_library");
    const char* type_items[] = {type_exe.c_str(), type_barekernel.c_str(), type_library.c_str()};
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##OutputType", &type_index, type_items, 3)) {
        proj.output_type = type_index == 1 ? AvaProjOutputType::kBareKernel
                            : type_index == 2 ? AvaProjOutputType::kLibrary
                                               : AvaProjOutputType::kExe;
        result.dirty = true;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.modules_path_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ProjectModulesPath", &proj.modules_path);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.dirty = true;
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.out_dir_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ProjectOutDir", &proj.out_dir);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.dirty = true;
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (DrawPathRow(util::Tr("project_properties.icon_label").c_str(),
                     util::Tr("project_properties.icon_hint").c_str(), proj.icon,
                     util::Tr("common.browse").c_str(), ProjectPropertiesBrowseField::kIcon, result)) {
        result.dirty = true;
    }
}

void DrawBuildTab(AvaProjFile& proj, AvaProjUserFile& user, ProjectPropertiesResult& result) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s",
                        util::Tr("project_properties.section_options").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (ImGui::Checkbox(util::Tr("build.obfuscate_label").c_str(), &proj.obfuscate)) result.dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("build.obfuscate_tooltip").c_str());
    }
    if (proj.obfuscate) {
        ImGui::Indent();
        if (ImGui::Checkbox(util::Tr("build.obfuscate_strings_label").c_str(), &proj.obfuscate_strings))
            result.dirty = true;
        if (ImGui::Checkbox(util::Tr("build.flatten_control_flow_label").c_str(), &proj.flatten_control_flow))
            result.dirty = true;
        ImGui::Unindent();
    }
    if (ImGui::Checkbox(util::Tr("build.zero_disk_label").c_str(), &proj.zero_disk)) result.dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("build.zero_disk_tooltip").c_str());
    }
    if (ImGui::Checkbox(util::Tr("build.debug_unencrypted_label").c_str(), &proj.debug_unencrypted))
        result.dirty = true;
    if (proj.debug_unencrypted) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            util::Tr("build.debug_unencrypted_warning").c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s",
                        util::Tr("project_properties.section_machine").c_str());
    ImGui::Separator();
    ImGui::TextDisabled("%s", util::Tr("project_properties.machine_note").c_str());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (DrawPathRow(util::Tr("build.ava_cli_path_label").c_str(), util::Tr("build.ava_cli_path_hint").c_str(),
                     user.ava_cli_path, util::Tr("common.browse").c_str(),
                     ProjectPropertiesBrowseField::kAvaCliPath, result)) {
        result.dirty = true;
    }
    if (DrawPathRow(util::Tr("build.key_file_label").c_str(), util::Tr("build.key_file_hint").c_str(),
                     user.key_file, util::Tr("common.browse").c_str(), ProjectPropertiesBrowseField::kKeyFile,
                     result)) {
        result.dirty = true;
    }
    if (DrawPathRow(util::Tr("build.vcpkg_root_label").c_str(), "", user.vcpkg_root,
                     util::Tr("common.browse").c_str(), ProjectPropertiesBrowseField::kVcpkgRoot, result)) {
        result.dirty = true;
    }
    if (DrawPathRow(util::Tr("build.compiler_path_hint_desktop").c_str(), "", user.compiler_path_desktop,
                     util::Tr("common.browse").c_str(), ProjectPropertiesBrowseField::kCompilerPathDesktop,
                     result)) {
        result.dirty = true;
    }
    if (DrawPathRow(util::Tr("build.compiler_path_hint_barekernel").c_str(), "", user.compiler_path_barekernel,
                     util::Tr("common.browse").c_str(), ProjectPropertiesBrowseField::kCompilerPathBarekernel,
                     result)) {
        result.dirty = true;
    }

    if (ImGui::Checkbox(util::Tr("build.force_so_label").c_str(), &user.force_so)) result.dirty = true;
    if (ImGui::Checkbox(util::Tr("build.force_runtime_label").c_str(), &user.force_runtime)) result.dirty = true;
}

void DrawReferencesTab(AvaProjFile& proj, const std::string& project_dir, ProjectPropertiesResult& result) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (ImGui::Button(util::Tr("project_properties.add_reference_button").c_str())) {
        result.browse_requested = ProjectPropertiesBrowseField::kReferenceFile;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (proj.references.empty()) {
        ImGui::TextDisabled("%s", util::Tr("project_properties.no_references").c_str());
        return;
    }

    std::optional<size_t> remove_index;
    for (size_t i = 0; i < proj.references.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextUnformatted(proj.references[i].include.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f + ImGui::GetCursorPosX());
        if (ImGui::Button(util::Tr("explorer.delete").c_str(), ImVec2(70.0f, 0.0f))) {
            remove_index = i;
        }
        ImGui::PopID();
    }
    if (remove_index) {
        proj.references.erase(proj.references.begin() + static_cast<long>(*remove_index));
        result.dirty = true;
    }
    (void)project_dir;
}

}  // namespace

ProjectPropertiesResult DrawProjectPropertiesPanel(ProjectPropertiesState& state, AvaProjFile& proj,
                                                     AvaProjUserFile& user, const std::string& project_dir,
                                                     ProjectPropertiesBrowseField browsed_field,
                                                     const std::string& browsed_value) {
    ProjectPropertiesResult result;

    if (browsed_field != ProjectPropertiesBrowseField::kNone && !browsed_value.empty()) {
        switch (browsed_field) {
            case ProjectPropertiesBrowseField::kIcon:
                proj.icon = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kAvaCliPath:
                user.ava_cli_path = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kKeyFile:
                user.key_file = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kVcpkgRoot:
                user.vcpkg_root = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kCompilerPathDesktop:
                user.compiler_path_desktop = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kCompilerPathBarekernel:
                user.compiler_path_barekernel = browsed_value;
                result.dirty = true;
                break;
            case ProjectPropertiesBrowseField::kReferenceFile: {
                const std::string relative = ToProjectRelativePath(project_dir, browsed_value);
                proj.references.push_back({relative.empty() ? browsed_value : relative});
                result.dirty = true;
                break;
            }
            case ProjectPropertiesBrowseField::kNone:
                break;
        }
    }

    const std::string title = util::Tr("project_properties.title") + "###project_properties";
    ImGui::SetNextWindowSize(ImVec2(520.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title.c_str(), &state.open)) {
        ImGui::End();
        return result;
    }

    if (ImGui::BeginTabBar("##ProjectPropertiesTabs")) {
        if (ImGui::BeginTabItem(util::Tr("project_properties.tab_application").c_str())) {
            DrawApplicationTab(proj, project_dir, result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(util::Tr("project_properties.tab_build").c_str())) {
            DrawBuildTab(proj, user, result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(util::Tr("project_properties.tab_references").c_str())) {
            DrawReferencesTab(proj, project_dir, result);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    return result;
}

}

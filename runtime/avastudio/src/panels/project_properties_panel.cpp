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

enum class ProjectKind { kConsole, kDesktopUi, kLibrary };

ProjectKind CurrentProjectKind(const AvaProjFile& proj) {
    if (proj.output_type == AvaProjOutputType::kLibrary) return ProjectKind::kLibrary;
    if (proj.uses_ui) return ProjectKind::kDesktopUi;
    return ProjectKind::kConsole;
}

void ApplyProjectKind(AvaProjFile& proj, ProjectKind kind) {
    switch (kind) {
        case ProjectKind::kConsole:
            proj.output_type = AvaProjOutputType::kExe;
            proj.uses_ui = false;
            break;
        case ProjectKind::kDesktopUi:
            proj.output_type = AvaProjOutputType::kExe;
            proj.uses_ui = true;
            break;
        case ProjectKind::kLibrary:
            proj.output_type = AvaProjOutputType::kLibrary;
            proj.uses_ui = false;
            proj.zero_disk = false;
            break;
    }
}

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
                        util::Tr("project_properties.target_label").c_str());
    int target_index = proj.target == AvaProjTarget::kBareKernel ? 1 : 0;
    const std::string target_desktop = util::Tr("build.target_desktop");
    const std::string target_barekernel = util::Tr("build.target_barekernel");
    const char* target_items[] = {target_desktop.c_str(), target_barekernel.c_str()};
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##Target", &target_index, target_items, 2)) {
        proj.target = target_index == 1 ? AvaProjTarget::kBareKernel : AvaProjTarget::kDesktop;
        if (proj.target == AvaProjTarget::kBareKernel) proj.uses_ui = false;
        result.dirty = true;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                        util::Tr("project_properties.kind_label").c_str());
    const bool target_is_barekernel = proj.target == AvaProjTarget::kBareKernel;

    const ProjectKind current_kind = CurrentProjectKind(proj);
    const std::string kind_console = util::Tr("project_properties.kind_console");
    const std::string kind_desktop_ui = util::Tr("project_properties.kind_desktop_ui");
    const std::string kind_library = util::Tr("project_properties.kind_library");
    auto KindLabel = [&](ProjectKind kind) -> const std::string& {
        switch (kind) {
            case ProjectKind::kConsole: return kind_console;
            case ProjectKind::kDesktopUi: return kind_desktop_ui;
            case ProjectKind::kLibrary: return kind_library;
        }
        return kind_console;
    };

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##ProjectKind", KindLabel(current_kind).c_str())) {
        for (ProjectKind candidate : {ProjectKind::kConsole, ProjectKind::kDesktopUi, ProjectKind::kLibrary}) {
            const bool disabled = candidate == ProjectKind::kDesktopUi && target_is_barekernel;
            ImGui::BeginDisabled(disabled);
            if (ImGui::Selectable(KindLabel(candidate).c_str(), current_kind == candidate)) {
                ApplyProjectKind(proj, candidate);
                result.dirty = true;
            }
            ImGui::EndDisabled();
            if (disabled && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", util::Tr("project_properties.kind_desktop_ui_barekernel_tooltip").c_str());
            }
        }
        ImGui::EndCombo();
    }
    if (current_kind == ProjectKind::kLibrary && target_is_barekernel) {
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            util::Tr("build.error_library_barekernel_not_supported").c_str());
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
    const bool zero_disk_disabled = proj.output_type == AvaProjOutputType::kLibrary;
    ImGui::BeginDisabled(zero_disk_disabled);
    if (ImGui::Checkbox(util::Tr("build.zero_disk_label").c_str(), &proj.zero_disk)) result.dirty = true;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr(zero_disk_disabled ? "build.zero_disk_disabled_library_tooltip"
                                                              : "build.zero_disk_tooltip").c_str());
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

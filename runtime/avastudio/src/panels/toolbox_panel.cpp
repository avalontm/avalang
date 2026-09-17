#include "panels/toolbox_panel.h"

#include "designer/toolbox_model.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "util/i18n.h"

#include <cfloat>
#include <string>
#include <vector>

namespace studio {

namespace {

void DrawCatalogRow(const designer::ComponentMetadata& entry) {
    ImGui::Selectable(entry.displayName.c_str(), false, ImGuiSelectableFlags_None);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kToolboxDragDropId, entry.type.c_str(), entry.type.size() + 1);
        ImGui::TextUnformatted(entry.displayName.c_str());
        ImGui::EndDragDropSource();
    }

    if (entry.designerCapabilities.isContainer) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("toolbox.container_tag").c_str());
    }
}

}

void DrawToolboxPanel(bool* p_open) {
    static std::string search_query;

    const std::string title = util::Tr("panel.toolbox.title") + "###toolbox";
    ImGui::Begin(title.c_str(), p_open);
    ImGui::TextDisabled("%s", util::Tr("toolbox.hint").c_str());

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##toolbox_search", util::Tr("toolbox.search_hint").c_str(), &search_query);
    ImGui::Separator();

    static const designer::ToolboxModel model;
    const std::vector<designer::ToolboxSection> sections = model.Sections(search_query);

    if (sections.empty()) {
        ImGui::TextDisabled("%s", util::Tr("toolbox.no_results").c_str());
    }

    for (size_t i = 0; i < sections.size(); ++i) {
        const designer::ToolboxSection& section = sections[i];
        if (i > 0) ImGui::Spacing();
        ImGui::TextColored(palette::FromHex(palette::kPrimaryLight), "%s", section.category.c_str());
        for (const designer::ComponentMetadata* entry : section.entries) {
            DrawCatalogRow(*entry);
        }
    }

    ImGui::End();
}

}

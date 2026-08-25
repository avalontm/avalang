#include "panels/toolbox_panel.h"

#include "design/component_catalog.h"
#include "imgui.h"
#include "palette.h"
#include "util/i18n.h"

#include <string>

namespace studio {

namespace {

void DrawCatalogRow(const design::ComponentTypeInfo& info) {
    ImGui::Selectable(info.display_name.c_str(), false, ImGuiSelectableFlags_None);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kToolboxDragDropId, info.type.c_str(),
                                   info.type.size() + 1);
        ImGui::TextUnformatted(info.display_name.c_str());
        ImGui::EndDragDropSource();
    }

    if (info.is_container) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("toolbox.container_tag").c_str());
    }
}

}

void DrawToolboxPanel(bool* p_open) {
    const std::string title = util::Tr("panel.toolbox.title") + "###toolbox";
    ImGui::Begin(title.c_str(), p_open);
    ImGui::TextDisabled("%s", util::Tr("toolbox.hint").c_str());
    ImGui::Separator();

    bool header_drawn = false;
    std::string last_category;
    for (const design::ComponentTypeInfo& info : design::GetComponentCatalog()) {
        if (!header_drawn || info.category != last_category) {
            if (header_drawn) ImGui::Spacing();
            ImGui::TextColored(palette::FromHex(palette::kPrimaryLight), "%s", info.category.c_str());
            header_drawn = true;
            last_category = info.category;
        }
        DrawCatalogRow(info);
    }

    ImGui::End();
}

}

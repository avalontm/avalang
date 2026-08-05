#include "panels/toolbox_panel.h"

#include "design/component_catalog.h"
#include "imgui.h"
#include "palette.h"

#include <string>

namespace studio {

namespace {

// One row: an icon-less label plus a small "container" hint tag for
// entries that accept children (Column/Row/Stack/Grid/Flex/Page) --
// mirrors the Explorer's folder-vs-file visual distinction so it's
// obvious at a glance what you can drop *into* once it's on the
// canvas, without needing a real icon set yet.
void DrawCatalogRow(const design::ComponentTypeInfo& info) {
    ImGui::Selectable(info.display_name.c_str(), false, ImGuiSelectableFlags_None);

    // The whole row is the drag source -- payload is just the type
    // string, the designer canvas is the one that knows how to turn
    // that into a real IComponent (via design::AddComponentNode) on drop.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kToolboxDragDropId, info.type.c_str(),
                                   info.type.size() + 1); // +1: NUL terminator, see designer_canvas.cpp
        ImGui::TextUnformatted(info.display_name.c_str());
        ImGui::EndDragDropSource();
    }

    if (info.is_container) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "(container)");
    }
}

} // namespace

void DrawToolboxPanel() {
    ImGui::Begin("Toolbox");
    ImGui::TextDisabled("Arrastrá un control al lienzo de Design");
    ImGui::Separator();

    // Catalog is already sorted by ComponentTypeInfo::order and carries
    // its own ComponentTypeInfo::category (both come from
    // data/component_catalog.csv, see B.6 in PLAN_UNIFICADO_AVAUI.md) --
    // this just draws a new header each time the category changes.
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

} // namespace studio

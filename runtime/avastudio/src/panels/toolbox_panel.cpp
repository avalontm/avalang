#include "panels/toolbox_panel.h"

#include "design/component_catalog.h"
#include "imgui.h"
#include "palette.h"

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
    // that into a real DesignNode (via design::MakeNode) on drop.
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

    // Catalog is already grouped Layout / Content / Interactive in
    // declaration order (see component_catalog.cpp) -- this just
    // re-derives a two-way Layout/Controles split from is_container
    // instead of adding a parallel "group" field to ComponentTypeInfo
    // for a distinction only this panel cares about. Tracked as a bool
    // rather than comparing header strings so grouping doesn't depend
    // on string-literal identity.
    bool header_drawn = false;
    bool last_was_container = false;
    for (const design::ComponentTypeInfo& info : design::GetComponentCatalog()) {
        if (!header_drawn || info.is_container != last_was_container) {
            if (header_drawn) ImGui::Spacing();
            ImGui::TextColored(palette::FromHex(palette::kPrimaryLight), "%s",
                                info.is_container ? "Layout" : "Controles");
            header_drawn = true;
            last_was_container = info.is_container;
        }
        DrawCatalogRow(info);
    }

    ImGui::End();
}

} // namespace studio

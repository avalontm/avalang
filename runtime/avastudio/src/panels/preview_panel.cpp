#include "panels/preview_panel.h"

#include "imgui.h"

namespace studio {

namespace {

// PROPERTIES_EDITABLE: this only reads from PreviewNode into
// PropertiesState, and leaves PropertiesState::editable at its default
// (false) -- Properties shows this selection read-only. Write-back
// exists now (see designer_canvas.cpp's ToPropertiesState and
// properties_panel.cpp/main.cpp, Fase 3 of 08_DESIGNER_VIEW_PLAN.md),
// but only for a real DesignNode backed by an actual .avaui file --
// this demo Component Tree has no source file to write into (see the
// note in engine_bridge.cpp), so it stays read-only on purpose.
void DrawNode(const EngineBridge::PreviewNode& node, std::optional<PropertiesState>& selected) {
    std::string label = node.type;
    if (!node.id.empty()) label += " (" + node.id + ")";

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        PropertiesState state;
        state.selected_component_type = node.type;
        state.selected_component_id = node.id;
        for (const auto& [key, value] : node.properties) {
            state.properties.push_back({key, value});
        }
        selected = std::move(state);
    }

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        for (const auto& child : node.children) {
            DrawNode(child, selected);
        }
        ImGui::TreePop();
    }
}

} // namespace

std::optional<PropertiesState> DrawPreviewPanel(const EngineBridge::PreviewNode& root) {
    std::optional<PropertiesState> selected;

    ImGui::Begin("Preview");
    ImGui::TextDisabled("Component Tree (demo -- see note in engine_bridge.cpp)");
    ImGui::Separator();
    DrawNode(root, selected);
    ImGui::End();

    return selected;
}

} // namespace studio

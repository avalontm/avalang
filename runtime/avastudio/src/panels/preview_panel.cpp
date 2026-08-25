#include "panels/preview_panel.h"

#include "imgui.h"

namespace studio {

namespace {

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

}

std::optional<PropertiesState> DrawPreviewPanel(const EngineBridge::PreviewNode& root, bool* p_open) {
    std::optional<PropertiesState> selected;

    ImGui::Begin("Preview###preview", p_open);
    ImGui::TextDisabled("Component Tree (demo -- see note in engine_bridge.cpp)");
    ImGui::Separator();
    DrawNode(root, selected);
    ImGui::End();

    return selected;
}

}

#include "panels/preview_panel.h"

#include "imgui.h"

namespace studio {

namespace {

// PROPERTIES_EDITABLE: this only reads from PreviewNode into
// PropertiesState -- there's no write-back into the .ava source yet.
// That's the "changing a property immediately updates the source file"
// piece from the vision doc; it needs a real script-backed tree (see
// the note in engine_bridge.cpp) before it can mean anything.
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
    ImGui::TextDisabled("Component Tree (demo -- ver nota en engine_bridge.cpp)");
    ImGui::Separator();
    DrawNode(root, selected);
    ImGui::End();

    return selected;
}

} // namespace studio

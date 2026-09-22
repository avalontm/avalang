#include "panels/preview_panel.h"

#include <algorithm>
#include <cctype>

#include "components/IComponent.h"
#include "designer/property_editor.h"
#include "designer/tools.h"
#include "designer/types.h"
#include "events/AutoBind.h"
#include "imgui.h"
#include "panels/designer_canvas.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::string LowerAscii(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string NodeIdProp(avalang::ui::IComponent* node) {
    const auto* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == designer::PropertyType::String) return idProp->AsString();
    return "";
}

PropertiesState ToPreviewState(avalang::ui::IComponent* node, int tab_id) {
    PropertiesState state;
    state.selected_component_type = LowerAscii(node->TypeName());
    state.selected_component_id = NodeIdProp(node);
    state.source_tab_id = tab_id;
    state.selected_node_id = node->NodeId();

    for (const std::string& name : node->PropertyNames()) {
        if (name == "id") continue;
        const auto* value = node->GetProperty(name);
        if (!value) continue;
        PropertyRow row{name, designer::FormatPropertyValue(*value)};
        if (avalang::ui::IsEventPropertyName(name)) {
            state.events.push_back(std::move(row));
        } else {
            state.properties.push_back(std::move(row));
        }
    }
    return state;
}

void DrawNode(avalang::ui::IComponent* node, int tab_id, std::optional<PropertiesState>& selected) {
    const std::string id_prop = NodeIdProp(node);
    std::string label = LowerAscii(node->TypeName());
    if (!id_prop.empty()) label += " (" + id_prop + ")";

    const std::vector<avalang::ui::IComponent*> children = node->Children();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::PushID(node->NodeId().c_str());
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        if (tab_id >= 0) {
            designer::SelectTool::Select(GetDesignerSelectionManager(tab_id), node->NodeId());
        }
        selected = ToPreviewState(node, tab_id);
    }

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        for (avalang::ui::IComponent* child : children) {
            DrawNode(child, tab_id, selected);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

}

std::optional<PropertiesState> DrawPreviewPanel(avalang::ui::IComponent* root, int tab_id, bool* p_open) {
    std::optional<PropertiesState> selected;

    ImGui::Begin("Preview###preview", p_open);
    if (root == nullptr) {
        ImGui::TextDisabled("%s", util::Tr("preview.empty").c_str());
    } else {
        DrawNode(root, tab_id, selected);
    }
    ImGui::End();

    return selected;
}

}

#include "panels/accessibility_panel.h"

#include <memory>
#include <string>
#include <vector>

#include "designer/accessibility_inspector.h"
#include "designer/property_editor.h"
#include "designer/selection_manager.h"
#include "imgui.h"
#include "palette.h"
#include "panels/designer_canvas.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::string RoleLabel(avalang::ui::accessibility::AccessibilityRole role) {
    using avalang::ui::accessibility::AccessibilityRole;
    switch (role) {
        case AccessibilityRole::Button: return "Button";
        case AccessibilityRole::Text: return "Text";
        case AccessibilityRole::Image: return "Image";
        case AccessibilityRole::Container: return "Container";
        case AccessibilityRole::TextInput: return "TextInput";
        case AccessibilityRole::CheckBox: return "CheckBox";
        case AccessibilityRole::RadioButton: return "RadioButton";
        case AccessibilityRole::ComboBox: return "ComboBox";
        case AccessibilityRole::Dialog: return "Dialog";
        case AccessibilityRole::Link: return "Link";
        case AccessibilityRole::ScrollView: return "ScrollView";
        case AccessibilityRole::Page: return "Page";
        default: return "Unknown";
    }
}

void JumpToNode(design::DesignDocument& doc, designer::SelectionManager* selection, avalang::ui::ComponentId id) {
    if (!doc.tree || !selection) {
        return;
    }
    if (avalang::ui::IComponent* node = doc.tree->FindById(id)) {
        selection->Select(node->NodeId());
    }
}

void DrawMissingLabelSection(design::DesignDocument& doc, designer::SelectionManager* selection,
                              const avalang::ui::accessibility::AccessibilityTree& tree) {
    const std::vector<designer::AccessibilityDiagnostic> diagnostics = designer::MissingLabelDiagnostics(tree);

    if (diagnostics.empty()) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.no_missing_labels").c_str());
        return;
    }

    for (const designer::AccessibilityDiagnostic& diagnostic : diagnostics) {
        ImGui::PushID(static_cast<int>(diagnostic.nodeId));

        avalang::ui::accessibility::AccessibilityNode* node = tree.FindById(diagnostic.nodeId);
        const std::string role = node ? RoleLabel(node->Role()) : RoleLabel(avalang::ui::accessibility::AccessibilityRole::Unknown);
        const std::string label = role + " \xE2\x80\x94 " + diagnostic.message;

        if (ImGui::Selectable(label.c_str())) {
            JumpToNode(doc, selection, diagnostic.nodeId);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("panel.accessibility.jump_to_node").c_str());
        }

        ImGui::PopID();
    }
}

void DrawTabOrderSection(design::DesignDocument& doc, designer::SelectionManager* selection,
                          const avalang::ui::accessibility::AccessibilityTree& tree) {
    const std::vector<avalang::ui::accessibility::AccessibilityNode*> order = designer::TabOrder(tree);

    if (order.empty()) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.no_tab_order").c_str());
        return;
    }

    for (size_t i = 0; i < order.size(); ++i) {
        avalang::ui::accessibility::AccessibilityNode* node = order[i];
        if (!node) {
            continue;
        }

        ImGui::PushID(static_cast<int>(node->Id()));

        const std::string role = RoleLabel(node->Role());
        const std::string descriptor = node->Label().empty() ? role : role + " \"" + node->Label() + "\"";
        const std::string label = std::to_string(i + 1) + ". " + descriptor;

        if (ImGui::Selectable(label.c_str())) {
            JumpToNode(doc, selection, node->Id());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("panel.accessibility.jump_to_node").c_str());
        }

        ImGui::PopID();
    }
}

std::string ReadColorProperty(avalang::ui::IComponent* node, const std::string& name) {
    const designer::PropertyValue* value = node->GetProperty(name);
    if (!value || value->Type() != designer::PropertyType::String) {
        return "";
    }
    return value->AsString();
}

void DrawContrastSection(design::DesignDocument& doc, const designer::SelectionManager* selection) {
    const std::string selected_node_id = selection ? selection->Primary() : std::string();
    avalang::ui::IComponent* node =
        selected_node_id.empty() ? nullptr : design::FindNodeById(doc.Root(), selected_node_id);
    if (!node) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.no_selection").c_str());
        return;
    }

    const std::string textValue = ReadColorProperty(node, "textColor");
    const std::string backgroundValue = ReadColorProperty(node, "backgroundColor");

    float unused_rgba[4];
    const bool textIsLiteral = !textValue.empty() && designer::TryParseColorValue(textValue, unused_rgba);
    const bool backgroundIsLiteral = !backgroundValue.empty() && designer::TryParseColorValue(backgroundValue, unused_rgba);

    if (!textIsLiteral || !backgroundIsLiteral) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.contrast_unavailable").c_str());
        return;
    }

    const avalang::ui::ThemeColor textColor(textValue);
    const avalang::ui::ThemeColor backgroundColor(backgroundValue);
    const double ratio = designer::TextContrastRatio(textColor, backgroundColor);
    const bool meetsNormal = designer::MeetsWcagAA(ratio, false);
    const bool meetsLarge = designer::MeetsWcagAA(ratio, true);

    ImGui::Text("%.2f:1", ratio);

    if (meetsNormal) {
        ImGui::TextColored(palette::FromHex(palette::kSuccess), "%s", util::Tr("panel.accessibility.wcag_pass").c_str());
    } else if (meetsLarge) {
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            util::Tr("panel.accessibility.wcag_large_only").c_str());
    } else {
        ImGui::TextColored(palette::FromHex(palette::kError), "%s", util::Tr("panel.accessibility.wcag_fail").c_str());
    }
}

}

void DrawAccessibilityInspectorPanel(design::DesignDocument& doc, int tab_id, bool* p_open) {
    designer::SelectionManager* selection = GetDesignerSelectionManager(tab_id);

    if (!ImGui::Begin(util::Tr("panel.accessibility.title").c_str(), p_open)) {
        ImGui::End();
        return;
    }

    if (!doc.tree || !doc.Root()) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.no_document").c_str());
        ImGui::End();
        return;
    }

    const std::unique_ptr<avalang::ui::accessibility::AccessibilityTree> tree =
        designer::BuildAccessibilityTree(doc.tree.get());
    if (!tree) {
        ImGui::TextDisabled("%s", util::Tr("panel.accessibility.no_document").c_str());
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader(util::Tr("panel.accessibility.missing_labels").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawMissingLabelSection(doc, selection, *tree);
    }

    if (ImGui::CollapsingHeader(util::Tr("panel.accessibility.tab_order").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawTabOrderSection(doc, selection, *tree);
    }

    if (ImGui::CollapsingHeader(util::Tr("panel.accessibility.contrast").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawContrastSection(doc, selection);
    }

    ImGui::End();
}

}

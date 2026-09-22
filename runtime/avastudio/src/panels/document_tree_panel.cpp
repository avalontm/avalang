#include "panels/document_tree_panel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "design/component_catalog.h"
#include "designer/document_commands.h"
#include "designer/drop_target.h"
#include "designer/tree_state.h"
#include "imgui.h"
#include "palette.h"
#include "panels/designer_canvas.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::unordered_map<int, designer::TreeState> g_tree_states;

struct RenameRequest {
    int tab_id = -1;
    std::string node_id;
    bool open = false;
};

RenameRequest g_rename_request;
char g_rename_buf[128] = "";

struct DeleteRequest {
    int tab_id = -1;
    std::string node_id;
    bool open = false;
};

DeleteRequest g_delete_request;

std::string LowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool IsContainerType(const std::string& type) {
    const design::ComponentTypeInfo* info = design::FindComponentType(LowerAscii(type));
    return info != nullptr && info->is_container;
}

design::DropZone RowDropZone(ImVec2 p0, ImVec2 p1, bool is_container) {
    const designer::LayoutRect rect{p0.x, p0.y, p1.x - p0.x, std::max(p1.y - p0.y, 1.0f)};
    const designer::LayoutPoint point{p0.x, ImGui::GetMousePos().y};
    return designer::ComputeDropZone(rect, point, is_container, false);
}

void DrawDropIndicator(design::DropZone zone, ImVec2 p0, ImVec2 p1) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 highlight = palette::U32FromHex(palette::kPrimary);
    switch (zone) {
        case design::DropZone::kInto:
            draw_list->AddRect(p0, p1, highlight, 2.0f, 0, 2.0f);
            break;
        case design::DropZone::kBefore:
            draw_list->AddLine(ImVec2(p0.x, p0.y), ImVec2(p1.x, p0.y), highlight, 2.0f);
            break;
        case design::DropZone::kAfter:
            draw_list->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), highlight, 2.0f);
            break;
    }
}

void HandleRowDragDrop(design::DesignDocument& doc, designer::CommandManager* commands,
                        designer::SelectionManager* selection, const designer::DocumentTreeNode& row,
                        ImVec2 p0, ImVec2 p1) {
    const bool is_container = IsContainerType(row.type);

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(kNodeMoveDragDropId, row.id.c_str(), row.id.size() + 1);
        ImGui::TextUnformatted(row.displayName.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (ImGui::AcceptDragDropPayload(kNodeMoveDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
            DrawDropIndicator(RowDropZone(p0, p1, is_container), p0, p1);
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeMoveDragDropId)) {
            const std::string moved_id(static_cast<const char*>(payload->Data));
            designer::ExecuteMoveComponent(commands, doc, selection, moved_id, row.id,
                                            RowDropZone(p0, p1, is_container));
        }
        ImGui::EndDragDropTarget();
    }
}

void OpenRename(int tab_id, const std::string& node_id, const std::string& current_name) {
    g_rename_request = {tab_id, node_id, true};
    std::snprintf(g_rename_buf, sizeof(g_rename_buf), "%s", current_name.c_str());
}

void DrawRenamePopup(design::DesignDocument& doc, int tab_id, designer::CommandManager* commands,
                      designer::SelectionManager* selection) {
    if (g_rename_request.tab_id != tab_id) return;

    if (g_rename_request.open) {
        ImGui::OpenPopup("##DocumentTreeRename");
        g_rename_request.open = false;
    }
    if (ImGui::BeginPopup("##DocumentTreeRename")) {
        ImGui::TextDisabled("%s", util::Tr("explorer.rename").c_str());
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##document_tree_rename", g_rename_buf, sizeof(g_rename_buf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        const bool confirm_clicked = ImGui::Button(util::Tr("explorer.rename").c_str());
        if ((enter || confirm_clicked) && g_rename_buf[0] != '\0') {
            designer::ExecuteSetProperty(commands, doc, selection, g_rename_request.node_id, "id", g_rename_buf);
            g_rename_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawDeleteConfirmPopup(design::DesignDocument& doc, int tab_id, designer::CommandManager* commands,
                             designer::SelectionManager* selection) {
    if (g_delete_request.tab_id != tab_id) return;

    const std::string delete_title = util::Tr("canvas.delete_title") + "##DocumentTreeDeleteConfirm";
    if (g_delete_request.open) {
        ImGui::OpenPopup(delete_title.c_str());
        g_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal(delete_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), g_delete_request.node_id);
        const std::string label = node ? designer::DisplayNameFor(node) : g_delete_request.node_id;
        ImGui::TextWrapped("%s", util::TrFormat("canvas.delete_confirm", {label}).c_str());
        ImGui::TextDisabled("%s", util::Tr("explorer.delete_undone").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float button_w = (ImGui::GetContentRegionAvail().x - spacing) / 2.0f;

        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kError, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(0xc93b3b));
        if (ImGui::Button(util::Tr("explorer.delete").c_str(), ImVec2(button_w, 0.0f))) {
            designer::ExecuteRemoveComponent(commands, doc, selection, g_delete_request.node_id);
            g_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button(util::Tr("common.cancel").c_str(), ImVec2(button_w, 0.0f))) {
            g_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawRow(design::DesignDocument& doc, int tab_id, const designer::DocumentTreeNode& row,
             designer::TreeState& tree_state, designer::CommandManager* commands,
             designer::SelectionManager* selection) {
    ImGui::PushID(row.id.c_str());
    ImGui::Indent(row.depth * 16.0f);

    if (row.hasChildren) {
        const bool expanded = tree_state.IsExpanded(row.id);
        if (ImGui::ArrowButton("##toggle", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
            tree_state.Toggle(row.id);
        }
        ImGui::SameLine(0.0f, 6.0f);
    } else {
        ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 1.0f));
        ImGui::SameLine(0.0f, 6.0f);
    }

    const bool is_selected = selection && selection->IsSelected(row.id);
    const bool renaming = g_rename_request.tab_id == tab_id && g_rename_request.node_id == row.id;

    if (renaming) {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##rename_inline", g_rename_buf, sizeof(g_rename_buf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
            if (g_rename_buf[0] != '\0') {
                designer::ExecuteSetProperty(commands, doc, selection, row.id, "id", g_rename_buf);
            }
            g_rename_request = {};
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            g_rename_request = {};
        }
    } else {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::Selectable(row.displayName.c_str(), is_selected,
                                                ImGuiSelectableFlags_AllowDoubleClick);
        const ImVec2 p1 = ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);

        if (clicked) {
            if (selection) selection->Select(row.id, ImGui::GetIO().KeyCtrl);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                OpenRename(tab_id, row.id, row.displayName);
            }
        }
        if (ImGui::IsItemHovered() && selection) {
            selection->SetHovered(row.id);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            if (selection) selection->Select(row.id);
        }

        if (ImGui::BeginPopupContextItem("##document_tree_row_context")) {
            if (selection) selection->Select(row.id);
            if (ImGui::MenuItem(util::Tr("explorer.rename").c_str(), "F2")) {
                OpenRename(tab_id, row.id, row.displayName);
            }
            if (ImGui::MenuItem(util::Tr("menu.edit.duplicate").c_str(), "Ctrl+D", false, row.depth > 0)) {
                const std::string created = designer::ExecuteDuplicateComponent(commands, doc, selection, row.id);
                if (!created.empty() && selection) selection->Select(created);
            }
            if (ImGui::MenuItem(util::Tr("explorer.delete").c_str(), "Del")) {
                g_delete_request = {tab_id, row.id, true};
            }
            ImGui::EndPopup();
        }

        HandleRowDragDrop(doc, commands, selection, row, p0, p1);
    }

    ImGui::Unindent(row.depth * 16.0f);
    ImGui::PopID();
}

}

void DrawDocumentTreePanel(design::DesignDocument& doc, int tab_id, bool* p_open) {
    if (!ImGui::Begin(util::Tr("panel.document_tree.title").c_str(), p_open)) {
        ImGui::End();
        return;
    }

    if (!doc.tree || !doc.Root()) {
        ImGui::TextDisabled("%s", util::Tr("panel.document_tree.no_document").c_str());
        ImGui::End();
        return;
    }

    designer::CommandManager* commands = GetDesignerCommandManager(tab_id);
    designer::SelectionManager* selection = GetDesignerSelectionManager(tab_id);
    designer::TreeState& tree_state = g_tree_states[tab_id];

    if (selection && !selection->Primary().empty()) {
        tree_state.ExpandAncestorsOf(selection->Primary(), doc.tree.get());
    }

    const std::vector<designer::DocumentTreeNode> visible = tree_state.VisibleNodes(doc.tree.get());
    for (const designer::DocumentTreeNode& row : visible) {
        DrawRow(doc, tab_id, row, tree_state, commands, selection);
    }

    if (selection && !selection->Primary().empty() && ImGui::IsWindowFocused() &&
        g_rename_request.tab_id != tab_id) {
        const std::string primary = selection->Primary();
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), primary);
            OpenRename(tab_id, primary, node ? designer::DisplayNameFor(node) : primary);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            g_delete_request = {tab_id, primary, true};
        }
    }

    DrawRenamePopup(doc, tab_id, commands, selection);
    DrawDeleteConfirmPopup(doc, tab_id, commands, selection);

    ImGui::End();
}

void ReleaseDocumentTreeState(int tab_id) {
    g_tree_states.erase(tab_id);
    if (g_rename_request.tab_id == tab_id) g_rename_request = {};
    if (g_delete_request.tab_id == tab_id) g_delete_request = {};
}

}

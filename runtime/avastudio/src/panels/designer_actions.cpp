#include "panels/designer_actions.h"

#include <algorithm>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "designer/command.h"
#include "designer/document_commands.h"
#include "designer/selection_manager.h"
#include "imgui.h"
#include "panels/designer_canvas.h"
#include "panels/designer_history.h"
#include "panels/editor_panel.h"
#include "shortcuts/shortcut_registry.h"

namespace studio {

namespace {

bool HasSelectedAncestor(const design::DesignDocument& document, avalang::ui::IComponent* node,
                         const std::vector<designer::NodeId>& selected) {
    for (avalang::ui::IComponent* parent = design::FindParentOf(document.Root(), node); parent != nullptr;
         parent = design::FindParentOf(document.Root(), parent)) {
        if (std::find(selected.begin(), selected.end(), parent->NodeId()) != selected.end()) {
            return true;
        }
    }
    return false;
}

std::vector<designer::NodeId> DuplicableSelection(const design::DesignDocument& document,
                                                  const designer::SelectionManager& selection) {
    const std::vector<designer::NodeId>& selected = selection.Selected();
    std::vector<designer::NodeId> result;
    for (const designer::NodeId& id : selected) {
        avalang::ui::IComponent* node = design::FindNodeById(document.Root(), id);
        if (node == nullptr || node == document.Root() || HasSelectedAncestor(document, node, selected)) {
            continue;
        }
        result.push_back(id);
    }
    return result;
}

}

bool CanDuplicateDesignerSelection(const EditorTab* tab) {
    if (!IsDesignerHistoryAvailable(tab)) return false;
    const designer::SelectionManager* selection = GetDesignerSelectionManager(tab->id);
    return selection != nullptr && !DuplicableSelection(tab->design, *selection).empty();
}

bool PollDesignerDuplicateShortcut() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return false;
    }
    return ShortcutRegistry::Instance().Pressed(ShortcutId::Duplicate, true);
}

bool DuplicateDesignerSelection(EditorTab& tab) {
    if (!IsDesignerHistoryAvailable(&tab)) return false;

    designer::CommandManager* commands = GetDesignerCommandManager(tab.id);
    designer::SelectionManager* selection = GetDesignerSelectionManager(tab.id);
    if (commands == nullptr || selection == nullptr || commands->InTransaction()) return false;

    const std::vector<designer::NodeId> sources = DuplicableSelection(tab.design, *selection);
    if (sources.empty()) return false;

    std::vector<designer::NodeId> created;
    commands->BeginTransaction("Duplicate");
    for (const designer::NodeId& id : sources) {
        const std::string duplicated = designer::ExecuteDuplicateComponent(commands, tab.design, selection, id);
        if (!duplicated.empty()) {
            created.push_back(duplicated);
        }
    }
    commands->EndTransaction();

    if (created.empty()) return false;
    selection->SelectMany(created);
    tab.dirty = true;
    return true;
}

}

#include "panels/designer_history.h"

#include "designer/command.h"
#include "designer/preview.h"
#include "imgui.h"
#include "panels/designer_canvas.h"
#include "panels/editor_panel.h"
#include "shortcuts/shortcut_registry.h"

namespace studio {

namespace {

designer::CommandManager* CommandsFor(const EditorTab* tab) {
    if (!IsDesignerHistoryAvailable(tab)) return nullptr;
    return GetDesignerCommandManager(tab->id);
}

}

bool IsDesignerHistoryAvailable(const EditorTab* tab) {
    return tab != nullptr && tab->is_avaui && tab->view_mode == TabViewMode::Design &&
           GetDesignerCanvasMode(tab->id) == designer::CanvasMode::Design;
}

bool CanDesignerUndo(const EditorTab* tab) {
    const designer::CommandManager* commands = CommandsFor(tab);
    return commands != nullptr && !commands->InTransaction() && commands->CanUndo();
}

bool CanDesignerRedo(const EditorTab* tab) {
    const designer::CommandManager* commands = CommandsFor(tab);
    return commands != nullptr && !commands->InTransaction() && commands->CanRedo();
}

DesignerHistoryAction PollDesignerHistoryShortcut() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return DesignerHistoryAction::kNone;
    }

    const ShortcutRegistry& shortcuts = ShortcutRegistry::Instance();
    if (shortcuts.Pressed(ShortcutId::Undo, true)) {
        return DesignerHistoryAction::kUndo;
    }
    if (shortcuts.Pressed(ShortcutId::Redo, true) || shortcuts.Pressed(ShortcutId::RedoAlternate, true)) {
        return DesignerHistoryAction::kRedo;
    }
    return DesignerHistoryAction::kNone;
}

bool ApplyDesignerHistory(EditorTab& tab, DesignerHistoryAction action) {
    designer::CommandManager* commands = CommandsFor(&tab);
    if (commands == nullptr || commands->InTransaction()) return false;

    if (action == DesignerHistoryAction::kUndo && commands->CanUndo()) {
        commands->Undo();
    } else if (action == DesignerHistoryAction::kRedo && commands->CanRedo()) {
        commands->Redo();
    } else {
        return false;
    }

    tab.dirty = true;
    return true;
}

}

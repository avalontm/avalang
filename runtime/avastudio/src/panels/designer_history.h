#pragma once

namespace studio {

struct EditorTab;

enum class DesignerHistoryAction {
    kNone,
    kUndo,
    kRedo,
};

bool IsDesignerHistoryAvailable(const EditorTab* tab);

bool CanDesignerUndo(const EditorTab* tab);

bool CanDesignerRedo(const EditorTab* tab);

DesignerHistoryAction PollDesignerHistoryShortcut();

bool ApplyDesignerHistory(EditorTab& tab, DesignerHistoryAction action);

}

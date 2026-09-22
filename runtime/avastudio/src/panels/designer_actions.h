#pragma once

namespace studio {

struct EditorTab;

bool CanDuplicateDesignerSelection(const EditorTab* tab);

bool PollDesignerDuplicateShortcut();

bool DuplicateDesignerSelection(EditorTab& tab);

}

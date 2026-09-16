#pragma once

#include <optional>
#include <string>
#include <vector>

namespace studio {

struct StateVarRow {
    std::string key;
    std::string value;
    std::string evaluated;
    bool has_error = false;
};

struct StateEditorState {
    std::vector<StateVarRow> variables;
    int source_tab_id = -1;
    bool has_code_behind = false;
};

enum class StateEditKind {
    kValue,
    kAddVariable,
    kRemoveVariable,
    kRenameVariable,
};

struct StateEdit {
    int tab_id = -1;
    StateEditKind kind = StateEditKind::kValue;
    std::string key;
    std::string new_key;
    std::string new_value;
};

std::optional<StateEdit> DrawStatePanel(StateEditorState& state, bool* p_open = nullptr);

}

#pragma once

#include <optional>

#include "panels/problems_panel.h"
#include "util/log_bridge.h"

namespace studio {

struct LogsState {
    int selection_anchor = -1;
    int selection_cursor = -1;
};

std::optional<ProblemsFileClickRequest> DrawLogsPanel(LogsState& state, LogBridge& log_bridge,
                                                        bool* p_open = nullptr);

}

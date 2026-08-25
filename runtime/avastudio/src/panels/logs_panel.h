#pragma once

#include "util/log_bridge.h"

namespace studio {

struct LogsState {
    int selection_anchor = -1;
    int selection_cursor = -1;
};

void DrawLogsPanel(LogsState& state, LogBridge& log_bridge, bool* p_open = nullptr);

}

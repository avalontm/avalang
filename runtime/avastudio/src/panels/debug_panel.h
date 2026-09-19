#pragma once

#include <optional>

#include "debug/debug_session.h"
#include "panels/problems_panel.h"

namespace studio {

struct DebugPanelResult {
    bool start_requested = false;
    std::optional<ProblemsFileClickRequest> file_click;
};

DebugPanelResult DrawDebugPanel(debug::DebugSession& session, bool* p_open = nullptr);

}

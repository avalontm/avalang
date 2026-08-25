#pragma once

#include <optional>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "imgui.h"
#include "panels/properties_panel.h"
#include "util/log_bridge.h"

namespace studio {

constexpr const char* kNodeMoveDragDropId = "AVAUI_NODE_MOVE";

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root = "",
                                                   int tab_id = -1,
                                                   std::string* out_generated_handler = nullptr,
                                                   LogBridge* log_bridge = nullptr);

void InvalidateDesignerVmCache(int tab_id);

}

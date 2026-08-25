#pragma once

#include <optional>

#include "engine/engine_bridge.h"
#include "panels/properties_panel.h"

namespace studio {

std::optional<PropertiesState> DrawPreviewPanel(const EngineBridge::PreviewNode& root, bool* p_open = nullptr);

}

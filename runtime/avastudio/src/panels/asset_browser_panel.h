#pragma once

#include <optional>
#include <string>

#include "panels/properties_panel.h"

namespace studio {

std::optional<PropertyEdit> DrawAssetBrowserPanel(const std::string& project_root, int tab_id,
                                                    bool* p_open = nullptr);

}

#pragma once

#include <string>
#include <vector>

#include "plugins/plugin_host.h"
#include "util/settings.h"

namespace studio {

void DrawSettingsPanel(StudioSettings& settings, const std::vector<RegisteredPanel>& settings_panels,
                        bool& out_settings_dirty, bool& out_browse_requested, const std::string& browsed_folder,
                        bool* p_open = nullptr);

}

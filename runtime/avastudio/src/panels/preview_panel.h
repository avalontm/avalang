#pragma once

#include <optional>

#include "panels/properties_panel.h"

namespace avalang::ui {
class IComponent;
}

namespace studio {

std::optional<PropertiesState> DrawPreviewPanel(avalang::ui::IComponent* root, int tab_id,
                                                  bool* p_open = nullptr);

}

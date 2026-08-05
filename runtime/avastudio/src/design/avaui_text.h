#pragma once

#include <string>
#include <vector>

#include "panels/properties_panel.h"
#include "components/IComponent.h"

namespace studio::design {

bool IsEventPropertyName(const std::string& name);

std::string WriteAvauiText(const avalang::ui::IComponent* root, const std::string& code_behind,
                            const std::vector<PropertyRow>& initial_state,
                            const std::vector<std::string>& imports);

} // namespace studio::design

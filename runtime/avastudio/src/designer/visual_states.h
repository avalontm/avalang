#pragma once

#include <string>

#include "theme/ProjectStyleOverrides.h"

namespace studio::designer {

enum class DesignerVisualState {
    Default,
    Hover,
    Focus,
    Active,
    Disabled,
};

std::string VisualStateToString(DesignerVisualState state);

avalang::ui::theme::ControlStyleOverride ResolveVisualState(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                              const std::string& typeLower,
                                                              DesignerVisualState state,
                                                              bool isPureLayoutContainer = false);

}

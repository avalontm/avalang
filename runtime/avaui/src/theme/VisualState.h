#pragma once

#include "theme/ProjectStyleOverrides.h"
#include "Export.h"

namespace avalang {
namespace ui {
namespace theme {

struct ComponentVisualState {
    bool disabled = false;
    bool pressed = false;
    bool focused = false;
    bool hovered = false;
    bool checked = false;
    bool selected = false;
};

AVA_UI_API ControlStyleOverride ResolveVisualStyle(const ProjectStyleSheet& styles,
                                                    const std::string& typeLower,
                                                    const ComponentVisualState& state);

}
}
}

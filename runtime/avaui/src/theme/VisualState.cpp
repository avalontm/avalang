#include "theme/VisualState.h"

namespace avalang {
namespace ui {
namespace theme {

ControlStyleOverride ResolveVisualStyle(const ProjectStyleSheet& styles,
                                         const std::string& typeLower,
                                         const ComponentVisualState& state) {
    if (!styles.HasAnyStateStyles()) {
        return ControlStyleOverride{};
    }

    ControlStyleOverride result;

    if (state.checked) {
        styles.ResolveState(typeLower, "checked").MergeOnto(result);
    }
    if (state.selected) {
        styles.ResolveState(typeLower, "selected").MergeOnto(result);
    }
    if (state.hovered) {
        styles.ResolveState(typeLower, "hover").MergeOnto(result);
    }
    if (state.focused) {
        styles.ResolveState(typeLower, "focus").MergeOnto(result);
    }
    if (state.pressed) {
        styles.ResolveState(typeLower, "active").MergeOnto(result);
    }
    if (state.disabled) {
        styles.ResolveState(typeLower, "disabled").MergeOnto(result);
    }

    return result;
}

}
}
}

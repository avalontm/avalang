#include "designer/visual_states.h"

namespace studio::designer {

std::string VisualStateToString(DesignerVisualState state) {
    switch (state) {
        case DesignerVisualState::Hover: return "hover";
        case DesignerVisualState::Focus: return "focus";
        case DesignerVisualState::Active: return "active";
        case DesignerVisualState::Disabled: return "disabled";
        case DesignerVisualState::Default: default: return std::string();
    }
}

avalang::ui::theme::ControlStyleOverride ResolveVisualState(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                              const std::string& typeLower,
                                                              DesignerVisualState state,
                                                              bool isPureLayoutContainer) {
    avalang::ui::theme::ControlStyleOverride result = styles.Resolve(typeLower, isPureLayoutContainer);

    if (state == DesignerVisualState::Default || !styles.HasAnyStateStyles()) {
        return result;
    }

    const avalang::ui::theme::ControlStyleOverride stateOverride =
        styles.ResolveState(typeLower, VisualStateToString(state));
    stateOverride.MergeOnto(result);

    return result;
}

}

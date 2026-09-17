#include "designer/animation_authoring.h"

namespace studio::designer {

std::string DescribeAnimation(const avalang::ui::parser::AnimationSpec& spec) {
    std::string text = spec.property.empty() ? std::string("(property)") : spec.property;

    if (!spec.fromRaw.empty() || !spec.toRaw.empty()) {
        text += ": " + (spec.fromRaw.empty() ? std::string("?") : spec.fromRaw) + " -> " +
                (spec.toRaw.empty() ? std::string("?") : spec.toRaw);
    }
    if (!spec.duration.empty()) {
        text += " (" + spec.duration + ")";
    }
    if (!spec.trigger.empty()) {
        text += " on " + spec.trigger;
    }
    if (!spec.easing.empty()) {
        text += " [" + spec.easing + "]";
    }

    return text;
}

std::unordered_map<avalang::ui::ComponentId, std::vector<avalang::ui::parser::AnimationSpec>>
GroupAnimationsByTarget(const std::vector<avalang::ui::parser::AnimationSpec>& specs) {
    std::unordered_map<avalang::ui::ComponentId, std::vector<avalang::ui::parser::AnimationSpec>> grouped;
    for (const avalang::ui::parser::AnimationSpec& spec : specs) {
        grouped[spec.target].push_back(spec);
    }
    return grouped;
}

std::string EasingLabel(avalang::ui::animation::EasingFunction easing) {
    switch (easing) {
        case avalang::ui::animation::EasingFunction::Linear: return "Linear";
        case avalang::ui::animation::EasingFunction::EaseIn: return "Ease In";
        case avalang::ui::animation::EasingFunction::EaseOut: return "Ease Out";
        case avalang::ui::animation::EasingFunction::EaseInOut: return "Ease In Out";
        default: return "Linear";
    }
}

}

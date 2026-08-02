#include "animation/Easing.h"

namespace avalang {
namespace ui {
namespace animation {

float ApplyEasing(EasingFunction easing, float t) {
    switch (easing) {
        case EasingFunction::EaseIn:
            return t * t;
        case EasingFunction::EaseOut:
            return t * (2.0f - t);
        case EasingFunction::EaseInOut:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
        case EasingFunction::Linear:
        default:
            return t;
    }
}

EasingFunction EasingFromString(const std::string& text) {
    if (text == "ease-in") return EasingFunction::EaseIn;
    if (text == "ease-out") return EasingFunction::EaseOut;
    if (text == "ease-in-out") return EasingFunction::EaseInOut;
    // "linear" and anything unrecognized/empty both fall back to
    // Linear -- soft gap, same policy as the rest of AnimationBinding's
    // text-parsing helpers (ParseAnimatableProperty, ParsePlaybackMode).
    return EasingFunction::Linear;
}

} // namespace animation
} // namespace ui
} // namespace avalang

#ifndef AVA_UI_ANIMATION_EASING_H
#define AVA_UI_ANIMATION_EASING_H

#include <string>

#include "Export.h"

// Fase 19.1 -- Animation: easing functions. Pure, no dependency on
// Scene Graph or any other phase -- same layer as IAnimatable.h (see
// UIModule.h, "Phase 19 (Animation)" entry).

namespace avalang {
namespace ui {
namespace animation {

enum class EasingFunction : unsigned char {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

// Applies `easing` to `t`, expected to already be in [0, 1] (callers,
// e.g. Timeline::SampleAt, clamp before calling this). Standard
// quadratic curves -- good enough for Fase 19's scope, not meant to
// match any particular external easing library bit-for-bit.
AVA_UI_API float ApplyEasing(EasingFunction easing, float t);

// Parses an authored easing name ("linear" | "ease-in" | "ease-out" |
// "ease-in-out") as used in an `animate` block's `easing = "..."`
// (see AvauiParser.h's AnimationSpec comment). Unrecognized/empty text
// falls back to Linear -- soft gap, same policy as
// AnimationBinding.cpp's other text-parsing helpers.
AVA_UI_API EasingFunction EasingFromString(const std::string& text);

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_ANIMATION_EASING_H

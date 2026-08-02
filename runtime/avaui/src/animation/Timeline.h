#ifndef AVA_UI_SRC_ANIMATION_TIMELINE_H
#define AVA_UI_SRC_ANIMATION_TIMELINE_H

#include <vector>

#include "animation/Easing.h"
#include "animation/IAnimatable.h"

// Phase 19.2 -- Animation: Timeline.
//
// Ordered list of (time, value, easing) keyframes for a single
// animatable property. Purely a data + sampling structure -- no
// notion of "playing", no clock of its own (that is
// AnimationController's job, Fase 19.3, which reads a shared
// IAnimationClock/elapsed cursor and calls SampleAt()). Not
// integrated into Scene Graph yet, per 19.2's own scope note in
// AVAUI_FASE19_PLAN.md -- testable standalone against synthetic
// keyframes.

namespace avalang {
namespace ui {
namespace animation {

struct Keyframe {
    float time = 0.0f; // seconds from the timeline's own start, >= 0
    AnimatableValue value;
    // Easing applied to the *segment* that ends at this keyframe (the
    // interpolation from the previous keyframe up to this one) --
    // mirrors how CSS/most animation tools attach easing to a
    // transition's destination, not its origin.
    EasingFunction easing = EasingFunction::Linear;
};

class Timeline {
public:
    Timeline() = default;

    // Appends a keyframe. Keyframes are kept sorted by `time`
    // (insertion order does not need to be time order) -- simplest
    // correct behavior for the handful of keyframes a real animation
    // uses (Fase 19.4 only ever builds a 2-keyframe from/to timeline
    // from a parsed `animate` block), not optimized for large N.
    void AddKeyframe(const Keyframe& keyframe);

    // Total duration: the time of the last keyframe (0 if empty or
    // single-keyframe).
    float Duration() const;

    // Samples the interpolated value at `timeSeconds` (clamped to
    // [0, Duration()]). Returns a default-constructed AnimatableValue
    // if no keyframes exist -- soft gap, not a hard failure, same
    // policy as the rest of Fase 14-18's "unrecognized input" handling
    // (an empty Timeline simply animates nothing).
    // - Before the first keyframe: holds the first keyframe's value.
    // - After the last keyframe: holds the last keyframe's value.
    // - Between two keyframes: Lerp()'d, eased by the later
    //   keyframe's `easing`.
    AnimatableValue SampleAt(float timeSeconds) const;

    const std::vector<Keyframe>& Keyframes() const { return keyframes_; }

private:
    std::vector<Keyframe> keyframes_;
};

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SRC_ANIMATION_TIMELINE_H

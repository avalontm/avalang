#include "animation/Timeline.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace animation {

void Timeline::AddKeyframe(const Keyframe& keyframe) {
    // Insert keeping keyframes_ sorted by time (stable w.r.t. equal
    // times -- last-added of a tied time wins position, harmless
    // since SampleAt() never needs to disambiguate two keyframes at
    // the exact same time in practice).
    auto it = std::upper_bound(
        keyframes_.begin(), keyframes_.end(), keyframe,
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
    keyframes_.insert(it, keyframe);
}

float Timeline::Duration() const {
    if (keyframes_.empty()) return 0.0f;
    return keyframes_.back().time;
}

AnimatableValue Timeline::SampleAt(float timeSeconds) const {
    if (keyframes_.empty()) return AnimatableValue();
    if (keyframes_.size() == 1) return keyframes_.front().value;

    float t = std::clamp(timeSeconds, 0.0f, Duration());

    if (t <= keyframes_.front().time) return keyframes_.front().value;
    if (t >= keyframes_.back().time) return keyframes_.back().value;

    // Find the first keyframe whose time is >= t -- the segment we're
    // in ends there and starts at the one before it.
    for (size_t i = 1; i < keyframes_.size(); ++i) {
        const Keyframe& prev = keyframes_[i - 1];
        const Keyframe& next = keyframes_[i];
        if (t > next.time) continue;

        float span = next.time - prev.time;
        // Two keyframes sharing a time (span == 0) is a degenerate
        // author error, not a structural one -- snap to `next` rather
        // than divide by zero (soft gap, same philosophy as
        // AvauiParser's semantic-gap handling).
        float segmentT = (span > 0.0f) ? (t - prev.time) / span : 1.0f;
        float eased = ApplyEasing(next.easing, segmentT);
        return Lerp(prev.value, next.value, eased);
    }

    // Unreachable given the early-outs above, but keeps this function
    // total rather than relying on falling off the end.
    return keyframes_.back().value;
}

} // namespace animation
} // namespace ui
} // namespace avalang

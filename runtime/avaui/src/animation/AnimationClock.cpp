#include "animation/AnimationClock.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace animation {

void AnimationClock::Tick(float deltaSeconds) {
    lastDelta_ = std::max(0.0f, deltaSeconds);
    elapsed_ += lastDelta_;
}

void AnimationClock::Reset() {
    elapsed_ = 0.0f;
    // lastDelta_ intentionally left as-is -- see IAnimationClock.h
    // class comment: Reset() only zeroes the running total, not the
    // most recent frame's delta.
}

std::unique_ptr<IAnimationClock> IAnimationClock::Create() {
    return std::make_unique<AnimationClock>();
}

} // namespace animation
} // namespace ui
} // namespace avalang

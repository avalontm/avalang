#ifndef AVA_UI_SRC_ANIMATION_IANIMATIONCLOCK_H
#define AVA_UI_SRC_ANIMATION_IANIMATIONCLOCK_H

#include <memory>

// Fase 19.1 -- Animation: single time source. Manual Tick(dt) frame
// clock -- no wall-clock/OS timer of its own, the caller (e.g. a
// window's frame loop) decides what `deltaSeconds` means and drives it
// every frame. Internal to ui/src/animation/ (see UIModule.h, "Phase
// 19 (Animation)" entry) -- AnimationController does not consume this
// directly today (it takes deltaSeconds as a parameter to Update()
// instead), this exists as the single time-source abstraction other
// animation-adjacent code can share instead of each reading its own
// clock.

namespace avalang {
namespace ui {
namespace animation {

class IAnimationClock {
public:
    static std::unique_ptr<IAnimationClock> Create();

    virtual ~IAnimationClock() = default;

    // Advances the clock by `deltaSeconds` (negative values clamped to
    // 0, same soft-gap policy as the rest of the animation stack).
    virtual void Tick(float deltaSeconds) = 0;

    // Most recent delta passed to Tick() (0 before the first Tick()).
    virtual float DeltaTime() const = 0;

    // Running total of all deltas since creation or the last Reset().
    virtual float Elapsed() const = 0;

    // Zeroes Elapsed() back to 0. Does NOT reset DeltaTime() -- the
    // most recently ticked frame delta is still meaningful information
    // about the current frame even if the running total was just
    // restarted (e.g. looping an animation without pretending the
    // previous frame never happened).
    virtual void Reset() = 0;
};

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SRC_ANIMATION_IANIMATIONCLOCK_H

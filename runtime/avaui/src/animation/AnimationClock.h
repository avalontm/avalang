#ifndef AVA_UI_SRC_ANIMATION_ANIMATIONCLOCK_H
#define AVA_UI_SRC_ANIMATION_ANIMATIONCLOCK_H

#include "animation/IAnimationClock.h"

namespace avalang {
namespace ui {
namespace animation {

// Concrete, manual-tick clock. `final`, not copyable -- same
// convention as Component/LayoutNode/SceneNode (single owner, no
// aliasing concerns).
class AnimationClock final : public IAnimationClock {
public:
    AnimationClock() = default;
    ~AnimationClock() override = default;

    AnimationClock(const AnimationClock&) = delete;
    AnimationClock& operator=(const AnimationClock&) = delete;

    void Tick(float deltaSeconds) override;
    float DeltaTime() const override { return lastDelta_; }
    float Elapsed() const override { return elapsed_; }
    void Reset() override;

private:
    float lastDelta_ = 0.0f;
    float elapsed_ = 0.0f;
};

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SRC_ANIMATION_ANIMATIONCLOCK_H

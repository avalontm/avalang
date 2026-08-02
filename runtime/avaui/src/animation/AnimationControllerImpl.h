#ifndef AVA_UI_SRC_ANIMATION_ANIMATIONCONTROLLERIMPL_H
#define AVA_UI_SRC_ANIMATION_ANIMATIONCONTROLLERIMPL_H

#include <unordered_map>

#include "animation/AnimationController.h"
#include "animation/Timeline.h"

namespace avalang {
namespace ui {
namespace animation {

struct ActiveAnimation {
    ComponentId target = 0;
    AnimatableProperty property = AnimatableProperty::Opacity;
    Timeline timeline; // always exactly 2 keyframes: from (t=0), to (t=duration)
    PlaybackMode mode = PlaybackMode::Once;
    bool playing = true;
    float elapsed = 0.0f; // seconds since this animation started/last looped
};

class AnimationControllerImpl final : public AnimationController {
public:
    explicit AnimationControllerImpl(scene::ISceneGraph* sceneGraph);
    ~AnimationControllerImpl() override = default;

    AnimationControllerImpl(const AnimationControllerImpl&) = delete;
    AnimationControllerImpl& operator=(const AnimationControllerImpl&) = delete;

    AnimationHandle Play(ComponentId target, AnimatableProperty property,
                         AnimatableValue from, AnimatableValue to,
                         float durationSeconds, EasingFunction easing,
                         PlaybackMode mode) override;

    void Pause(AnimationHandle handle) override;
    void Resume(AnimationHandle handle) override;
    void Stop(AnimationHandle handle) override;
    bool IsPlaying(AnimationHandle handle) const override;

    void Update(float deltaSeconds) override;

private:
    // Applies the sampled value to whichever SceneNode `target`
    // resolves to (no-op, soft gap, if FindNode() returns null -- see
    // AnimationController.h class comment).
    void ApplyToScene(const ActiveAnimation& anim, const AnimatableValue& sampled);

    scene::ISceneGraph* sceneGraph_; // non-owning, see AnimationController::Create()
    std::unordered_map<AnimationHandle, ActiveAnimation> active_;
    AnimationHandle nextHandle_ = 1; // 0 is kInvalidAnimationHandle
};

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SRC_ANIMATION_ANIMATIONCONTROLLERIMPL_H

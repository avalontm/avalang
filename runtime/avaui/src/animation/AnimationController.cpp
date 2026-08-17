#include "animation/AnimationControllerImpl.h"

#include <algorithm>
#include <cmath>

#include "scene/ISceneNode.h"

namespace avalang {
namespace ui {
namespace animation {

AnimationControllerImpl::AnimationControllerImpl(scene::ISceneGraph* sceneGraph)
    : sceneGraph_(sceneGraph) {}

AnimationHandle AnimationControllerImpl::Play(ComponentId target, AnimatableProperty property,
                                              AnimatableValue from, AnimatableValue to,
                                              float durationSeconds, EasingFunction easing,
                                              PlaybackMode mode) {
    if (durationSeconds <= 0.0f) return kInvalidAnimationHandle;
    AnimatableKind expected = KindOf(property);
    if (from.kind != expected || to.kind != expected) return kInvalidAnimationHandle;

    Timeline timeline;
    timeline.AddKeyframe(Keyframe{0.0f, from, EasingFunction::Linear});
    timeline.AddKeyframe(Keyframe{durationSeconds, to, easing});

    ActiveAnimation anim;
    anim.target = target;
    anim.property = property;
    anim.timeline = std::move(timeline);
    anim.mode = mode;
    anim.playing = true;
    anim.elapsed = 0.0f;

    AnimationHandle handle = nextHandle_++;
    active_.emplace(handle, std::move(anim));

    auto it = active_.find(handle);
    if (it != active_.end()) {
        ApplyToScene(it->second, it->second.timeline.SampleAt(0.0f));
    }

    return handle;
}

void AnimationControllerImpl::Pause(AnimationHandle handle) {
    auto it = active_.find(handle);
    if (it == active_.end()) return;
    it->second.playing = false;
}

void AnimationControllerImpl::Resume(AnimationHandle handle) {
    auto it = active_.find(handle);
    if (it == active_.end()) return;
    it->second.playing = true;
}

void AnimationControllerImpl::Stop(AnimationHandle handle) {
    active_.erase(handle);
}

bool AnimationControllerImpl::IsPlaying(AnimationHandle handle) const {
    auto it = active_.find(handle);
    if (it == active_.end()) return false;
    return it->second.playing;
}

void AnimationControllerImpl::Update(float deltaSeconds) {
    float dt = (deltaSeconds > 0.0f) ? deltaSeconds : 0.0f;

    for (auto& entry : active_) {
        ActiveAnimation& anim = entry.second;
        if (!anim.playing) continue;

        anim.elapsed += dt;
        float duration = anim.timeline.Duration();
        float cursor = 0.0f;

        switch (anim.mode) {
            case PlaybackMode::Once:
                cursor = std::min(anim.elapsed, duration);
                if (anim.elapsed >= duration) {

                    anim.playing = false;
                }
                break;
            case PlaybackMode::Loop:
                cursor = (duration > 0.0f) ? std::fmod(anim.elapsed, duration) : 0.0f;
                break;
            case PlaybackMode::PingPong: {
                if (duration <= 0.0f) {
                    cursor = 0.0f;
                    break;
                }
                float period = 2.0f * duration;
                float m = std::fmod(anim.elapsed, period);
                cursor = (m <= duration) ? m : (period - m);
                break;
            }
        }

        ApplyToScene(anim, anim.timeline.SampleAt(cursor));
    }
}

void AnimationControllerImpl::ApplyToScene(const ActiveAnimation& anim,
                                            const AnimatableValue& sampled) {
    if (!sceneGraph_) return;
    auto node = sceneGraph_->FindNode(anim.target);

    if (!node) return;

    switch (anim.property) {
        case AnimatableProperty::Opacity:
            node->SetOpacity(sampled.f);
            break;
        case AnimatableProperty::Position: {
            scene::Transform t = node->LocalTransform();
            t.position = sampled.v2;
            node->SetLocalTransform(t);
            break;
        }
        case AnimatableProperty::Scale: {
            scene::Transform t = node->LocalTransform();
            t.scale = sampled.v2;
            node->SetLocalTransform(t);
            break;
        }
        case AnimatableProperty::Rotation: {
            scene::Transform t = node->LocalTransform();
            t.rotation = sampled.f;
            node->SetLocalTransform(t);
            break;
        }
    }

    node->MarkDirty();
}

std::unique_ptr<AnimationController> AnimationController::Create(scene::ISceneGraph* sceneGraph) {
    return std::make_unique<AnimationControllerImpl>(sceneGraph);
}

} // namespace animation
} // namespace ui
} // namespace avalang

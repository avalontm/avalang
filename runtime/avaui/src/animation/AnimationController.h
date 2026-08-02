#ifndef AVA_UI_ANIMATION_ANIMATIONCONTROLLER_H
#define AVA_UI_ANIMATION_ANIMATIONCONTROLLER_H

#include <cstdint>
#include <memory>

#include "Export.h"
#include "Fwd.h"
#include "animation/IAnimatable.h"
#include "animation/Easing.h"
// Fwd.h's forward declarations put ISceneGraph directly in
// avalang::ui (Phase 7 stale entry, never updated when ISceneGraph
// moved under avalang::ui::scene) -- include the real header instead
// of relying on it, same as commands/SceneCommandWalker.h already
// does for the same reason.
#include "scene/ISceneGraph.h"

// Fase 19.3 -- Animation: AnimationController.
//
// Owns zero or more in-flight animations, samples them every Update(dt)
// and writes the interpolated value into the target ISceneNode via
// SetLocalTransform()/SetOpacity() (Phase 7, frozen since F13) --
// explicitly calls MarkDirty() afterwards, since SetOpacity() does not
// on its own (see ui/src/scene/SceneNode.h). Depends on scene/ (via
// scene::ISceneGraph, forward-declared in Fwd.h) but nothing above it
// in the Components -> Layout -> ... dependency direction.
//
// Soft-gap philosophy, same as the rest of the animation stack: Play()
// with an invalid duration/mismatched AnimatableValue kind returns
// kInvalidAnimationHandle instead of asserting; Update() on a target
// ComponentId that doesn't resolve to a live SceneNode simply has
// nothing to write into that frame, the animation's cursor still
// advances.

namespace avalang {
namespace ui {
namespace animation {

using AnimationHandle = std::uint64_t;
constexpr AnimationHandle kInvalidAnimationHandle = 0;

enum class PlaybackMode : unsigned char {
    Once,     // plays from `from` to `to` once, then holds the final value
    Loop,     // restarts from `from` every `durationSeconds`
    PingPong, // alternates from->to->from->... forever
};

class AVA_UI_API AnimationController {
public:
    // `sceneGraph` is non-owning -- must outlive the returned
    // controller. Passing nullptr is valid (Update() simply has
    // nothing to write into; useful for tests that only care about
    // Play()/Pause()/Stop() bookkeeping).
    static std::unique_ptr<AnimationController> Create(scene::ISceneGraph* sceneGraph);

    virtual ~AnimationController() = default;

    // Starts a new animation of `property` on `target`, from `from` to
    // `to` over `durationSeconds`, eased by `easing`, repeating
    // according to `mode`. Returns kInvalidAnimationHandle (and starts
    // nothing) if durationSeconds <= 0 or if `from`/`to` don't match
    // the AnimatableKind that `property` expects (see
    // IAnimatable.h's KindOf()). Applies `from` immediately so the
    // target doesn't wait for the next Update() to show its starting
    // value.
    virtual AnimationHandle Play(ComponentId target, AnimatableProperty property,
                                  AnimatableValue from, AnimatableValue to,
                                  float durationSeconds, EasingFunction easing,
                                  PlaybackMode mode = PlaybackMode::Once) = 0;

    // No-op if `handle` is not an active animation (already Stop()'d,
    // or never valid).
    virtual void Pause(AnimationHandle handle) = 0;
    virtual void Resume(AnimationHandle handle) = 0;

    // Removes the animation entirely -- unlike a PlaybackMode::Once
    // animation reaching its end, Stop() means IsPlaying() returns
    // false AND the animation no longer exists to resume.
    virtual void Stop(AnimationHandle handle) = 0;

    virtual bool IsPlaying(AnimationHandle handle) const = 0;

    // Advances every active, non-paused animation by `deltaSeconds`
    // (negative values clamped to 0) and re-applies its sampled value
    // to the scene. Call once per frame.
    virtual void Update(float deltaSeconds) = 0;
};

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_ANIMATION_ANIMATIONCONTROLLER_H

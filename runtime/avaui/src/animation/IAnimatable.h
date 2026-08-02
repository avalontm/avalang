#ifndef AVA_UI_ANIMATION_IANIMATABLE_H
#define AVA_UI_ANIMATION_IANIMATABLE_H

#include <glm/glm.hpp>

#include "Export.h"

// Fase 19.1 -- Animation: generic interpolable value.
//
// Pure data + Lerp(), no dependency on Scene Graph or any other phase --
// this is the lowest layer of the animation stack (see UIModule.h,
// "Phase 19 (Animation)" entry). AnimatableProperty enumerates which
// ISceneNode/Transform field an animation targets; KindOf() is the
// single source of truth for which AnimatableKind each property expects,
// so AnimationController::Play() and AnimationBinding's text parsing
// both validate against it instead of duplicating the mapping.

namespace avalang {
namespace ui {
namespace animation {

enum class AnimatableKind : unsigned char {
    Float,
    Vec2,
};

// Which ISceneNode/Transform field an animation writes into (see
// AnimationControllerImpl::ApplyToScene).
enum class AnimatableProperty : unsigned char {
    Opacity,
    Position,
    Scale,
    Rotation,
};

// Returns the AnimatableKind a given property expects -- Opacity and
// Rotation are scalars (Float), Position and Scale are glm::vec2.
AVA_UI_API AnimatableKind KindOf(AnimatableProperty property);

// Tagged union: `kind` says which of `f`/`v2` is meaningful. Only one
// of the two named factories should be used to construct a value --
// direct aggregate construction is intentionally not the primary API,
// so `kind` and the active member never fall out of sync by accident.
struct AVA_UI_API AnimatableValue {
    AnimatableKind kind = AnimatableKind::Float;
    float f = 0.0f;
    glm::vec2 v2 = {0.0f, 0.0f};

    static AnimatableValue FromFloat(float value);
    static AnimatableValue FromVec2(glm::vec2 value);
};

// Linearly interpolates between `a` and `b` at `t` (not clamped to
// [0, 1] here -- callers, e.g. Timeline::SampleAt, are responsible for
// clamping `t` before calling this). `a` and `b` are expected to share
// the same `kind`; if they don't, `a`'s kind wins (soft gap, same
// policy as the rest of the animation stack -- see
// AnimationController.h class comment).
AVA_UI_API AnimatableValue Lerp(const AnimatableValue& a, const AnimatableValue& b, float t);

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_ANIMATION_IANIMATABLE_H

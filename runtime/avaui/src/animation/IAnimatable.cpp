#include "animation/IAnimatable.h"

namespace avalang {
namespace ui {
namespace animation {

AnimatableKind KindOf(AnimatableProperty property) {
    switch (property) {
        case AnimatableProperty::Position:
        case AnimatableProperty::Scale:
            return AnimatableKind::Vec2;
        case AnimatableProperty::Opacity:
        case AnimatableProperty::Rotation:
        default:
            return AnimatableKind::Float;
    }
}

AnimatableValue AnimatableValue::FromFloat(float value) {
    AnimatableValue result;
    result.kind = AnimatableKind::Float;
    result.f = value;
    return result;
}

AnimatableValue AnimatableValue::FromVec2(glm::vec2 value) {
    AnimatableValue result;
    result.kind = AnimatableKind::Vec2;
    result.v2 = value;
    return result;
}

AnimatableValue Lerp(const AnimatableValue& a, const AnimatableValue& b, float t) {
    AnimatableValue result;
    result.kind = a.kind;
    if (a.kind == AnimatableKind::Vec2) {
        result.v2 = a.v2 + (b.v2 - a.v2) * t;
    } else {
        result.f = a.f + (b.f - a.f) * t;
    }
    return result;
}

} // namespace animation
} // namespace ui
} // namespace avalang

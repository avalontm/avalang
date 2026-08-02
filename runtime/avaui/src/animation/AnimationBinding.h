#ifndef AVA_UI_ANIMATION_ANIMATIONBINDING_H
#define AVA_UI_ANIMATION_ANIMATIONBINDING_H

#include <string>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "Fwd.h"
#include "animation/AnimationController.h"
#include "events/IEventDispatcher.h"
#include "parser/AvauiParser.h"
// Fwd.h only forward-declares IState -- WireAnimations() calls
// state->Subscribe() itself, so it needs the complete type here, not
// just a pointer-compatible declaration.
#include "state/IState.h"

// Fase 19.4 -- Animation: WireAnimations.
//
// Resolves the raw, un-interpreted parser::AnimationSpec structs
// produced by AvauiParser (Phase 14, extended 19.4 -- see
// AvauiParser.h's AnimationSpec comment) into real
// animation::AnimatableProperty/AnimatableValue/EasingFunction/
// PlaybackMode enums, and subscribes each spec's `trigger`:
//
//   - trigger == "click": subscribes a click handler on `dispatcher`
//     for the spec's target ComponentId. Requires a non-null
//     `dispatcher` -- specs with a click trigger are silently skipped
//     (soft gap) if it's null.
//   - trigger == a `states` key: subscribes to that IState's changes.
//     A Bool state triggers the animation only when it becomes true;
//     any other PropertyType triggers on every change. Unknown/absent
//     keys are silently skipped (soft gap) -- the spec stays
//     manual-only (Play() still callable directly on the returned/
//     owned AnimationController, just nothing auto-triggers it).
//   - trigger empty: manual-only, nothing is subscribed.
//
// `controller` and `dispatcher` are both non-owning and must outlive
// any triggers this function subscribes (dispatcher for click
// triggers, the IState pointers inside `states` for state triggers).
// Passing a null `controller` is a no-op (nothing to play into).
//
// This intentionally lives in animation/ rather than parser/: parser/
// must not depend on animation/ per the Components -> Layout -> ...
// dependency direction documented in Fwd.h (animation/ depends on
// scene/, parser/ deliberately does not).

namespace avalang {
namespace ui {
namespace animation {

AVA_UI_API void WireAnimations(const std::vector<parser::AnimationSpec>& specs,
                                AnimationController* controller,
                                events::IEventDispatcher* dispatcher,
                                const std::unordered_map<std::string, IState*>& states);

} // namespace animation
} // namespace ui
} // namespace avalang

#endif // AVA_UI_ANIMATION_ANIMATIONBINDING_H

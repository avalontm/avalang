#include "animation/AnimationBinding.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace avalang {
namespace ui {
namespace animation {

namespace {

// property = "opacity" | "position" | "scale" | "rotation" -- unknown
// text falls back to Opacity (soft gap, same policy as
// AvauiParser::CanonicalTypeName's fallback for an unknown TypeName).
AnimatableProperty ParseAnimatableProperty(const std::string& text) {
    if (text == "position") return AnimatableProperty::Position;
    if (text == "scale") return AnimatableProperty::Scale;
    if (text == "rotation") return AnimatableProperty::Rotation;
    return AnimatableProperty::Opacity;
}

// mode = "once" | "loop" | "pingpong" -- unknown/empty falls back to
// Once, matching AnimationController's own default parameter.
PlaybackMode ParsePlaybackMode(const std::string& text) {
    if (text == "loop") return PlaybackMode::Loop;
    if (text == "pingpong" || text == "ping-pong") return PlaybackMode::PingPong;
    return PlaybackMode::Once;
}

// "1.0" -> Float, or "x,y" -> Vec2, chosen by `property`'s expected
// kind (KindOf()) rather than by sniffing the text for a comma -- an
// author who writes `from = "10,20"` for an opacity animation gets a
// soft-gap zero rather than an accidental Vec2/Float mismatch that
// AnimationController::Play() would then reject outright.
AnimatableValue ParseAnimatableValueText(const std::string& raw, AnimatableProperty property) {
    if (KindOf(property) == AnimatableKind::Vec2) {
        float x = 0.0f, y = 0.0f;
        size_t comma = raw.find(',');
        if (comma != std::string::npos) {
            x = std::strtof(raw.substr(0, comma).c_str(), nullptr);
            y = std::strtof(raw.substr(comma + 1).c_str(), nullptr);
        } else if (!raw.empty()) {
            // A single number for a Vec2 property (position/scale)
            // applies to both axes -- convenience for the common
            // uniform-scale case ("to = 1.2" meaning "1.2, 1.2").
            x = y = std::strtof(raw.c_str(), nullptr);
        }
        return AnimatableValue::FromVec2({x, y});
    }
    return AnimatableValue::FromFloat(raw.empty() ? 0.0f : std::strtof(raw.c_str(), nullptr));
}

// Registry of click-trigger handlers, kept alive for the process --
// same pattern as controls::Button's g_buttonCallbacks (Fase 17).
// WireAnimations has no matching Unwire() in this phase, so nothing
// ever erases from this vector; the handler's lambda only captures
// value types and non-owning pointers, so leaking it for the
// process's lifetime is harmless.
std::vector<std::unique_ptr<events::IEventHandler>> g_clickHandlers;
std::mutex g_clickHandlersMutex;

class ClickTriggerHandler : public events::IEventHandler {
public:
    explicit ClickTriggerHandler(std::function<void()> onClick) : onClick_(std::move(onClick)) {}

    void OnEvent(events::IEvent* event) override {
        if (!event || event->Type() != events::EventType::Click) return;
        if (onClick_) onClick_();
    }

private:
    std::function<void()> onClick_;
};

} // namespace

void WireAnimations(const std::vector<parser::AnimationSpec>& specs,
                    AnimationController* controller,
                    events::IEventDispatcher* dispatcher,
                    const std::unordered_map<std::string, IState*>& states) {
    if (!controller) return;

    for (const auto& spec : specs) {
        AnimatableProperty property = ParseAnimatableProperty(spec.property);
        AnimatableValue from = ParseAnimatableValueText(spec.fromRaw, property);
        AnimatableValue to = ParseAnimatableValueText(spec.toRaw, property);
        float duration = spec.duration.empty() ? 0.0f : std::strtof(spec.duration.c_str(), nullptr);
        if (duration <= 0.0f) duration = 0.3f; // default, matches Timeline's own "no keyframes" gap philosophy: never silently produce a no-op animation
        EasingFunction easing = spec.easing.empty() ? EasingFunction::Linear
                                                     : EasingFromString(spec.easing);
        PlaybackMode mode = ParsePlaybackMode(spec.mode);
        ComponentId target = spec.target;

        auto playFn = [controller, target, property, from, to, duration, easing, mode]() {
            controller->Play(target, property, from, to, duration, easing, mode);
        };

        if (spec.trigger == "click") {
            if (!dispatcher) continue;
            auto handler = std::make_unique<ClickTriggerHandler>(playFn);
            dispatcher->Subscribe(target, events::EventType::Click, handler.get());
            std::lock_guard<std::mutex> lock(g_clickHandlersMutex);
            g_clickHandlers.push_back(std::move(handler));
        } else if (!spec.trigger.empty()) {
            auto it = states.find(spec.trigger);
            if (it == states.end() || !it->second) {
                // Soft gap: named trigger doesn't resolve to a live
                // IState -- spec stays manual-only. See class comment
                // in AnimationBinding.h.
                continue;
            }
            IState* state = it->second;
            state->Subscribe([playFn](const PropertyValue& newValue) {
                if (newValue.Type() == PropertyType::Bool) {
                    if (newValue.AsBool()) playFn();
                } else {
                    playFn();
                }
            });
        }
        // trigger empty: manual-only, nothing to subscribe.
    }
}

} // namespace animation
} // namespace ui
} // namespace avalang

#ifndef AVA_UI_STATE_ISTATE_H
#define AVA_UI_STATE_ISTATE_H

#include <cstddef>
#include <functional>
#include <memory>

#include "Export.h"
#include "Fwd.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

// Phase 4 -- State System. A single reactive value cell: holds one
// PropertyValue (Phase 2's property type, reused here rather than
// duplicated -- a state's value is exactly the kind of thing that
// ends up in a component's property bag) and notifies subscribers
// whenever it changes. No rendering, no component/layout awareness of
// its own -- connecting a state to a specific component's property is
// StateBinding's job, not IState's.
//
// Owned by whoever creates it (typically a script/controller, or a
// StateBinding that also owns it -- see StateBinding::Create). Unlike
// ComponentTree/LayoutEngine, there is no central registry: a state is
// just a value with observers, so it doesn't need one.
class AVA_UI_API IState {
public:
    static std::unique_ptr<IState> Create(PropertyValue initial = PropertyValue());

    virtual ~IState() = default;

    virtual const PropertyValue& Value() const = 0;

    // Replaces the current value. If the new value is equal to the
    // current one (same PropertyType and same underlying value),
    // this is a no-op -- no notification fires, since nothing changed.
    virtual void Set(PropertyValue value) = 0;

    using ChangeHandler = std::function<void(const PropertyValue& newValue)>;

    // Registers `handler` to be called with the new value every time
    // Set() actually changes it. Returns a subscription id to pass to
    // Unsubscribe(). Does not invoke `handler` with the current value
    // immediately -- read Value() directly if you need that.
    virtual std::size_t Subscribe(ChangeHandler handler) = 0;

    // No-op if `subscriptionId` doesn't correspond to an active
    // subscription (e.g. already unsubscribed).
    virtual void Unsubscribe(std::size_t subscriptionId) = 0;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_STATE_ISTATE_H

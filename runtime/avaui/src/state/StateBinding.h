#ifndef AVA_UI_STATE_STATEBINDING_H
#define AVA_UI_STATE_STATEBINDING_H

#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/IComponent.h"
#include "state/IState.h"

namespace avalang {
namespace ui {

// Phase 4 -- one-way binding from an IState to a single IComponent
// property. On creation, writes the state's current value into
// `component`'s `propertyName` property immediately (so the component
// starts in sync, not just after the first future change), then
// subscribes to the state so every subsequent change re-applies the
// same way -- via IComponent::SetProperty, the same entry point any
// other caller uses (Phase 2). StateBinding never reads back from the
// component and never touches layout/rendering; "property updates" in
// this phase means exactly this one push, state -> component.
//
// RAII: does not own `state` or `component` (both are non-owning,
// caller-supplied pointers, same convention as the rest of the Fwd.h
// dependency graph), but owns the *subscription* -- destroying the
// StateBinding unsubscribes from the state, so it is always safe to
// destroy a binding before, after, or alongside the state or the
// component it points to.
class AVA_UI_API StateBinding {
public:
    // `state` and `component` must outlive the returned StateBinding
    // (unsubscription happens in ~StateBinding, not before). Either
    // may be null, in which case Create() is a no-op binding that
    // never pushes anything.
    static std::unique_ptr<StateBinding> Create(IState* state, IComponent* component, std::string propertyName);

    virtual ~StateBinding() = default;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_STATE_STATEBINDING_H

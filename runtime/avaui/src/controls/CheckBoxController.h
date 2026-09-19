#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"

#include <functional>
#include <memory>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

// Optional hooks that let the host own the checked state when `isChecked` is a
// state binding (`isChecked = agreed`) instead of a literal bool.
//
// Without them the controller can only read a literal Bool, so a bound checkbox
// always looked "unchecked" to it: the first click toggled from the wrong value
// and then overwrote the binding with a literal, disconnecting the checkbox
// from its state variable for good.
struct CheckBoxBinding {
    // The state the user currently SEES (binding already evaluated).
    std::function<bool(IComponent* checkBox)> resolveChecked;

    // Called with the new state on a toggle. Return true when the host wrote
    // it back to the bound variable; the component's `isChecked` property is
    // then left untouched, so the binding survives. Return false to fall back
    // to the default behavior (overwrite `isChecked` with the literal).
    std::function<bool(IComponent* checkBox, bool newChecked)> commitChecked;
};

class AVA_UI_API CheckBoxController {
public:
    explicit CheckBoxController(events::IEventDispatcher& dispatcher);
    ~CheckBoxController();

    CheckBoxController(const CheckBoxController&) = delete;
    CheckBoxController& operator=(const CheckBoxController&) = delete;

    void Attach(IComponent* root);

    // May be called before or after Attach(); handlers read it at click time.
    void SetBinding(CheckBoxBinding binding);

private:
    class Handler;

    void AttachRecursive(IComponent* node);

    events::IEventDispatcher& dispatcher_;
    CheckBoxBinding binding_;
    std::vector<std::unique_ptr<Handler>> handlers_;
};

}
}
}

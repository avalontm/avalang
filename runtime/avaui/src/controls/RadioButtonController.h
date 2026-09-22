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

// Optional hooks that let the host own the selection when `isSelected` is a
// state binding (`selected = {planBasic}`) instead of a literal bool.
//
// Without them the controller can only read/write a literal Bool on the
// component itself (see SelectRadioButton/GetRadioButtonSelected), so a
// bound RadioButton never actually updated its state variable, and group
// siblings bound to their own variables (`planBasic` / `planPro`) never got
// deselected -- only the plain-property g_groups bookkeeping did.
struct RadioButtonBinding {
    // The state the user currently SEES (binding already evaluated).
    std::function<bool(IComponent* radioButton)> resolveSelected;

    // Called on activation (click / activation key). The host is expected to
    // set this radio button's bound variable to true AND deselect every
    // other RadioButton in the same `group` (mirroring what
    // SelectRadioButton does for the unbound case). Return true when the
    // host handled it; the component's own `isSelected` property is then
    // left untouched so the binding survives. Return false to fall back to
    // the default behavior (SelectRadioButton on the plain property).
    std::function<bool(IComponent* radioButton)> commitSelected;
};

class AVA_UI_API RadioButtonController {
public:
    explicit RadioButtonController(events::IEventDispatcher& dispatcher);
    ~RadioButtonController();

    RadioButtonController(const RadioButtonController&) = delete;
    RadioButtonController& operator=(const RadioButtonController&) = delete;

    void Attach(IComponent* root);

    // May be called before or after Attach(); handlers read it at click time.
    void SetBinding(RadioButtonBinding binding);

private:
    class Handler;

    void AttachRecursive(IComponent* node);

    events::IEventDispatcher& dispatcher_;
    RadioButtonBinding binding_;
    std::vector<std::unique_ptr<Handler>> handlers_;
};

}
}
}

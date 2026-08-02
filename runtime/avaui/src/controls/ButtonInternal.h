#pragma once

#include "Fwd.h"
#include "controls/Button.h"

namespace avalang::ui::controls::internal {

/**
 * Internal accessor: retrieve a Button's click callback (if bound).
 * Used by EventDispatcher (Phase 5) to invoke callbacks after Click events.
 * 
 * Returns nullptr if no callback is bound for this button.
 * 
 * Thread-safe.
 */
ButtonClickCallback* GetButtonClickCallback(ComponentId buttonId);

}  // namespace avalang::ui::controls::internal

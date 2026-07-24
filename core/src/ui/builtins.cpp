#include "ui/builtins.h"

namespace ava {
namespace ui {

void RegisterUIBuiltins(AvaVM* /*vm*/) {
    // Intentionally empty — see builtins.h. Nothing in this snapshot
    // called RegisterUIBuiltins, so there is no evidence of what native
    // functions it originally registered. Left as a documented no-op
    // rather than guessing at an API surface.
}

} // namespace ui
} // namespace ava

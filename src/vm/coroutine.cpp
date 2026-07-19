#include "coroutine.h"

namespace ava {

// TODO: once the compiler emits YIELD/RESUME (DESIGN.md build order, step 7),
// implement VM::Resume(Coroutine&, args) here: swap VM::frames_ with
// coroutine.frames, run the interpreter loop until either RETURN (status =
// Dead) or YIELD (status = Suspended, values captured), then swap the
// original frame stack back in. This file is the intended home for that
// logic so it stays colocated with the Coroutine type it manipulates.

} // namespace ava

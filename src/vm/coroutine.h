#ifndef AVA_VM_COROUTINE_H
#define AVA_VM_COROUTINE_H

#include <vector>
#include "value.h"
#include "vm.h"

namespace ava {

enum class CoStatus { Suspended, Running, Dead };

// A coroutine is just its own independent call-frame stack. Resuming it
// means swapping this stack in as the VM's active stack and continuing the
// same interpreter loop; yielding means saving pc/registers right where
// they are and swapping back to the resumer. No OS threads, no native
// recursion -- see DESIGN.md section 5.
struct Coroutine : Object {
    Value entry;             // the function this coroutine runs
    std::vector<CallFrame> frames;
    CoStatus status = CoStatus::Suspended;
};

} // namespace ava

#endif // AVA_VM_COROUTINE_H

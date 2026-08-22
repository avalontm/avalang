#include "vm.h"
#include "vm_internal.h"

namespace ava {

void HandleFrameError(VM& vm, size_t frame_idx, const avastd::exception& e) {
    // Frame index is already passed in from the dispatcher
    // This is a placeholder for error decoration logic
    // The actual exception handling with source mapping is done in ExecuteFrame's catch blocks
    (void)vm;
    (void)frame_idx;
    (void)e;
}

} // namespace ava
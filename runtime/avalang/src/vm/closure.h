#ifndef AVA_VM_CLOSURE_H
#define AVA_VM_CLOSURE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "value.h"
#include "proto.h"

namespace ava {

// A boxed value shared between a closure and the frame that created it,
// so writes made after the closure escapes its creating scope are still
// visible (standard "cell" / upvalue-box technique).
// Upvalue vive por shared_ptr (Closure::upvalues), no por el refcounting
// de Value/Object -- ver GcObjectKind::Upvalue en value.h. El sweep de
// ciclos tiene que excluirlo por completo de los candidatos a `delete`
// directo: borrarlo el mismo, ademas del shared_ptr que ya lo posee,
// seria un double-free.
struct Upvalue : Object {
    Value* location;
    Value value;
    // Register index within the originating CallFrame this upvalue was
    // opened against, -1 once closed (or if never open, e.g. deserialized).
    // Used by CallFrame::open_upvalues to intern one Upvalue per (frame,
    // register) -- so sibling closures created in the same frame that
    // capture the same local share this exact object -- and by
    // VM::CloseUpvalues to find which open upvalues belong to a frame
    // that's about to be popped.
    int reg_index = -1;
    Upvalue(Value* loc = nullptr) : Object(GcObjectKind::Upvalue), location(loc) {}
};

struct Closure : Object {
    Closure() : Object(GcObjectKind::Function) {}
    avastd::shared_ptr<Proto> proto;
    avastd::vector<avastd::shared_ptr<Upvalue>> upvalues;
};

} // namespace ava

#endif // AVA_VM_CLOSURE_H

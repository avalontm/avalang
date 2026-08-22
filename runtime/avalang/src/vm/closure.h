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
    Upvalue(Value* loc = nullptr) : Object(GcObjectKind::Upvalue), location(loc) {}
};

struct Closure : Object {
    Closure() : Object(GcObjectKind::Function) {}
    avastd::shared_ptr<Proto> proto;
    avastd::vector<avastd::shared_ptr<Upvalue>> upvalues;
};

} // namespace ava

#endif // AVA_VM_CLOSURE_H

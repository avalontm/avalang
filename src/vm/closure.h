#ifndef AVA_VM_CLOSURE_H
#define AVA_VM_CLOSURE_H

#include <memory>
#include <vector>
#include "value.h"
#include "proto.h"

namespace ava {

// A boxed value shared between a closure and the frame that created it,
// so writes made after the closure escapes its creating scope are still
// visible (standard "cell" / upvalue-box technique).
struct Upvalue : Object {
    Value* location;
    Value value;
    Upvalue(Value* loc = nullptr) : location(loc) {}
};

struct Closure : Object {
    std::shared_ptr<Proto> proto;
    std::vector<std::shared_ptr<Upvalue>> upvalues;
};

} // namespace ava

#endif // AVA_VM_CLOSURE_H

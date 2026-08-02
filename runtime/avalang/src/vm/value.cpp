#include "value.h"

namespace ava {

void Retain(const Value& v) {
    if (v.IsRefCounted() && v.obj) {
        v.obj->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void Release(const Value& v) {
    if (v.IsRefCounted() && v.obj) {
        if (v.obj->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete v.obj;
        }
    }
}

// AvaRef.id is the Object* reinterpreted as an integer handle. This keeps
// ava_value_t a flat POD struct with no pointer-typed field that bindings
// would need special-case marshalling for (works uniformly as uint64 across
// C#/Python/Java FFI layers).
ava_value_t ToC(const Value& v) {
    ava_value_t out{};
    out.type = static_cast<AvaValueType>(v.type);
    switch (v.type) {
        case ValueType::Nil:    break;
        case ValueType::Bool:   out.as.b = v.b ? 1 : 0; break;
        case ValueType::Number: out.as.n = v.n; break;
        default:
            out.as.ref.id = reinterpret_cast<uint64_t>(v.obj);
            break;
    }
    return out;
}

Value FromC(const ava_value_t& v) {
    Value out;
    out.type = static_cast<ValueType>(v.type);
    switch (out.type) {
        case ValueType::Nil:    break;
        case ValueType::Bool:   out.b = v.as.b != 0; break;
        case ValueType::Number: out.n = v.as.n; break;
        default:
            out.obj = reinterpret_cast<Object*>(v.as.ref.id);
            break;
    }
    return out;
}

} // namespace ava

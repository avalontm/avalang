#ifndef AVA_BUILTINS_BUILTIN_SHARED_H
#define AVA_BUILTINS_BUILTIN_SHARED_H

#include "vm/value.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

Value MakeNilV();

avastd::string TypeName(const Value& v);

// Same conversion `print`/`str()` already used before this file
// existed -- kept here, not duplicated, so any other builtin that
// needs to render a Value as text (e.g. System.Console.WriteLine)
// stays byte-for-byte consistent with `print`'s output instead of
// drifting into a second, subtly different formatter.
avastd::string ToDisplayString(const Value& v);

double AsNumber(const Value& v);

avastd::vector<Value> CollectItems(const avastd::vector<Value>& args);
avastd::vector<Value> ArgsToValues(const ava_value_t* args, size_t count);
bool LessThan(const Value& a, const Value& b);

// Fase 7 bugfix (moved here from system_module.cpp so every builtin/API
// file can share the one definition instead of each hand-rolling its own
// copy) -- see the Retain()/FromC() comment in vm/value.cpp for the full
// story. A Value freshly constructed locally (String/List/Dict) that gets
// exposed to the caller via ToC() must be Retain()-ed first: ToC() is a
// deliberately non-retaining, flat POD cast (see value.cpp), so without
// this the temporary Value's RAII destructor releases the brand-new
// object (refcount 1 -> 0, delete) the instant the expression ends --
// and the ava_value_t already returned is a dangling handle to freed
// memory before the caller even receives it. Nil/Bool/Number don't need
// this (Value::IsRefCounted() is false for those); only sites returning
// a newly-created String/List/Dict do.
inline ava_value_t ToCNew(const Value& v) {
    Retain(v);
    return ToC(v);
}

} // namespace ava

#endif // AVA_BUILTINS_BUILTIN_SHARED_H

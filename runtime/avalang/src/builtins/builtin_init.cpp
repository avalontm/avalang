#include "builtin.h"
#include "builtin_natives.h"
#include "vm/vm.h"

using namespace ava;

extern "C" {

// Declared in builtin.h, called from public/src/c_api.cpp's
// ava_vm_create(). Registers every global (bare-name) native — as
// opposed to RegisterBuiltinMethods, which registers dotted methods
// like str.upper(). See builtin_natives.h for the full list; `str`,
// `len`, and `type` are load-bearing for core/src/compiler/compiler.cpp
// (f-string joins, for-loops, dynamic-for dispatch).
BUILTIN_API void RegisterBuiltinGlobals(AvaVM* vm) {
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);

    raw_vm->RegisterNative("type", builtin_type, nullptr);
    raw_vm->RegisterNative("str", builtin_str, nullptr);
    raw_vm->RegisterNative("int", builtin_int, nullptr);
    raw_vm->RegisterNative("float", builtin_float, nullptr);
    raw_vm->RegisterNative("print", builtin_print, nullptr);
    raw_vm->RegisterNative("input", builtin_input, nullptr);

    raw_vm->RegisterNative("abs", builtin_abs, nullptr);
    raw_vm->RegisterNative("round", builtin_round, nullptr);
    raw_vm->RegisterNative("floor", builtin_floor, nullptr);
    raw_vm->RegisterNative("ceil", builtin_ceil, nullptr);
    raw_vm->RegisterNative("min", builtin_min, nullptr);
    raw_vm->RegisterNative("max", builtin_max, nullptr);
    raw_vm->RegisterNative("pow", builtin_pow, nullptr);
    raw_vm->RegisterNative("sqrt", builtin_sqrt, nullptr);
    raw_vm->RegisterNative("sum", builtin_sum, nullptr);
    raw_vm->RegisterNative("sorted", builtin_sorted, nullptr);
    raw_vm->RegisterNative("reversed", builtin_reversed, nullptr);
    raw_vm->RegisterNative("any", builtin_any, nullptr);
    raw_vm->RegisterNative("all", builtin_all, nullptr);
    raw_vm->RegisterNative("len", builtin_len, nullptr);
    raw_vm->RegisterNative("range", builtin_range, nullptr);
    raw_vm->RegisterNative("slice", builtin_slice, nullptr);

    // Required by every `import` statement -- see builtin_import's doc
    // comment in builtin_natives.h for why this must be registered here.
    raw_vm->RegisterNative("__import__", builtin_import, nullptr);

    // Memoria cruda para decodificar retornos de `extern` que en C son
    // char*/char** (ver builtin_mem.cpp) -- p.ej. mysql_error() o un
    // MYSQL_ROW.
    raw_vm->RegisterNative("mem_is_null", builtin_mem_is_null, nullptr);
    raw_vm->RegisterNative("mem_peek_string", builtin_mem_peek_string, nullptr);
    raw_vm->RegisterNative("mem_peek_ptr", builtin_mem_peek_ptr, nullptr);
}

// Declared in builtin.h but (per grep across this snapshot) never
// actually called from anywhere — ava_vm_create() calls
// RegisterBuiltinMethods + RegisterBuiltinGlobals directly, not this.
// Kept as a convenience entry point that does both, for any external
// embedder that wants a single call.
BUILTIN_API void RegisterAll(AvaVM* vm) {
    RegisterBuiltinMethods(vm);
    RegisterBuiltinGlobals(vm);
}

} // extern "C"

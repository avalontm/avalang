#ifndef AVA_BUILTINS_BUILTIN_NATIVES_H
#define AVA_BUILTINS_BUILTIN_NATIVES_H

#include "avalang.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define BUILTIN_API __declspec(dllexport)
#else
  #define BUILTIN_API __attribute__((visibility("default")))
#endif

// Global (non-method) builtins. Registered via VM::RegisterNative so
// scripts call them as bare names: type(x), str(x), len(x), print(x), ...
// Signatures and names per PROGRESS.md's builtins table, plus `print`
// (used pervasively by scripts/*.ava but otherwise undeclared anywhere in
// this snapshot) and `type`/`str`/`len` (confirmed required by
// core/src/compiler/compiler.cpp, which emits GETGLOBAL for exactly these
// three names).

ava_value_t builtin_type(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_int(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_float(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_print(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

ava_value_t builtin_abs(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_round(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_floor(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_ceil(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_min(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_max(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_pow(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_sqrt(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_sum(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_sorted(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_reversed(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_any(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_all(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_len(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_range(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // AVA_BUILTINS_BUILTIN_NATIVES_H

#ifndef AVA_BUILTINS_BUILTIN_H
#define AVA_BUILTINS_BUILTIN_H

#include "avalang.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define BUILTIN_API __declspec(dllexport)
#else
  #define BUILTIN_API __attribute__((visibility("default")))
#endif

ava_value_t builtin_str_upper(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_lower(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_split(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_trim(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_contains(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_replace(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_length(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_indexOf(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_startsWith(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_endsWith(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_str_substring(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

ava_value_t builtin_list_append(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_pop(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_push(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_insert(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_remove(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_length(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_list_contains(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

ava_value_t builtin_dict_keys(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_dict_values(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_dict_items(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_dict_length(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_dict_containsKey(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

ava_value_t builtin_coroutine(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);
ava_value_t builtin_resume(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

BUILTIN_API void RegisterBuiltinMethods(AvaVM* vm);

#ifdef __cplusplus
}
#endif

#endif // AVA_BUILTINS_BUILTIN_H
#include "builtin.h"
#include "vm/vm.h"

extern "C" {

void RegisterBuiltinMethods(AvaVM* vm) {
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    
    raw_vm->RegisterBuiltinMethod("str_upper", builtin_str_upper, nullptr);
    raw_vm->RegisterBuiltinMethod("str_lower", builtin_str_lower, nullptr);
    raw_vm->RegisterBuiltinMethod("str_split", builtin_str_split, nullptr);
    raw_vm->RegisterBuiltinMethod("str_trim", builtin_str_trim, nullptr);
    raw_vm->RegisterBuiltinMethod("str_contains", builtin_str_contains, nullptr);
    raw_vm->RegisterBuiltinMethod("str_replace", builtin_str_replace, nullptr);
    raw_vm->RegisterBuiltinMethod("str_length", builtin_str_length, nullptr);
    raw_vm->RegisterBuiltinMethod("str_indexOf", builtin_str_indexOf, nullptr);
    raw_vm->RegisterBuiltinMethod("str_startsWith", builtin_str_startsWith, nullptr);
    raw_vm->RegisterBuiltinMethod("str_endsWith", builtin_str_endsWith, nullptr);
    raw_vm->RegisterBuiltinMethod("str_substring", builtin_str_substring, nullptr);
    
    raw_vm->RegisterBuiltinMethod("list_append", builtin_list_append, nullptr);
    raw_vm->RegisterBuiltinMethod("list_pop", builtin_list_pop, nullptr);
    raw_vm->RegisterBuiltinMethod("list_push", builtin_list_push, nullptr);
    raw_vm->RegisterBuiltinMethod("list_insert", builtin_list_insert, nullptr);
    raw_vm->RegisterBuiltinMethod("list_remove", builtin_list_remove, nullptr);
    raw_vm->RegisterBuiltinMethod("list_length", builtin_str_length, nullptr);
    raw_vm->RegisterBuiltinMethod("list_contains", builtin_list_contains, nullptr);
    
    raw_vm->RegisterBuiltinMethod("dict_keys", builtin_dict_keys, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_values", builtin_dict_values, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_items", builtin_dict_items, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_length", builtin_dict_length, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_containsKey", builtin_dict_containsKey, nullptr);
}

}
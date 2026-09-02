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
    raw_vm->RegisterBuiltinMethod("list_removeAt", builtin_list_removeAt, nullptr);
    raw_vm->RegisterBuiltinMethod("list_length", builtin_list_length, nullptr);
    raw_vm->RegisterBuiltinMethod("list_contains", builtin_list_contains, nullptr);
    raw_vm->RegisterBuiltinMethod("list_indexOf", builtin_list_indexOf, nullptr);
    raw_vm->RegisterBuiltinMethod("list_sort", builtin_list_sort, nullptr);
    raw_vm->RegisterBuiltinMethod("list_reverse", builtin_list_reverse, nullptr);
    raw_vm->RegisterBuiltinMethod("list_clear", builtin_list_clear, nullptr);
    raw_vm->RegisterBuiltinMethod("list_copy", builtin_list_copy, nullptr);
    raw_vm->RegisterBuiltinMethod("list_join", builtin_list_join, nullptr);
    
    raw_vm->RegisterBuiltinMethod("dict_keys", builtin_dict_keys, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_values", builtin_dict_values, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_items", builtin_dict_items, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_length", builtin_dict_length, nullptr);
    raw_vm->RegisterBuiltinMethod("dict_containsKey", builtin_dict_containsKey, nullptr);

    // coroutine/resume/set_timeout/sleep_async/clear_timeout/delay se
    // registraban acá hasta la Fase 1 de PLAN_VALIDACION_ESTATICA.md.
    // Son bare natives (coroutine(...), no obj.coroutine()), no métodos
    // dotted como el resto de esta función — se movieron a
    // RegisterBuiltinGlobals (builtin_init.cpp), que ahora es la única
    // fuente de nombres bare (ver AVA_BUILTIN_GLOBALS en
    // builtin_names.h). RegisterAll/c_api.cpp siguen llamando ambas
    // funciones juntas, así que el orden de registro real no cambia.
}

}
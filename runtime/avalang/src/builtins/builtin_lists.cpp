#include "builtin.h"
#include <vector>

namespace {

ava_value_t MakeNil() {
    ava_value_t v{};
    v.type = AVA_NIL;
    return v;
}

ava_value_t MakeBool(bool b) {
    ava_value_t v{};
    v.type = AVA_BOOL;
    v.as.b = b;
    return v;
}

ava_value_t MakeNumber(double n) {
    ava_value_t v{};
    v.type = AVA_NUMBER;
    v.as.n = n;
    return v;
}

ava_value_t MakeList() {
    return ava_list_create(nullptr);
}

}

extern "C" {

ava_value_t builtin_list_append(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    ava_list_append(vm, args[0], args[1]);
    return args[0];
}

ava_value_t builtin_list_pop(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t len = ava_list_length(vm, args[0]);
    if (len == 0) return MakeNil();
    
    ava_value_t last = ava_list_get(vm, args[0], len - 1);
    ava_value_t result;
    result.type = last.type;
    result.as = last.as;
    
    ava_list_remove(vm, args[0], len - 1);
    return result;
}

ava_value_t builtin_list_push(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    ava_list_append(vm, args[0], args[1]);
    return args[0];
}

ava_value_t builtin_list_insert(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 3) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t pos = static_cast<size_t>(args[1].as.n);
    ava_list_insert(vm, args[0], pos, args[2]);
    return args[0];
}

ava_value_t builtin_list_remove(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t pos = static_cast<size_t>(args[1].as.n);
    size_t len = ava_list_length(vm, args[0]);
    if (pos >= len) return MakeNil();
    
    ava_value_t item = ava_list_get(vm, args[0], pos);
    ava_list_remove(vm, args[0], pos);
    
    ava_value_t result;
    result.type = item.type;
    result.as = item.as;
    return result;
}

ava_value_t builtin_list_length(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    return MakeNumber(static_cast<double>(ava_list_length(vm, args[0])));
}

ava_value_t builtin_list_contains(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t len = ava_list_length(vm, args[0]);
    for (size_t i = 0; i < len; i++) {
        ava_value_t item = ava_list_get(vm, args[0], i);
        if (item.type == args[1].type && item.as.n == args[1].as.n) {
            return MakeBool(true);
        }
    }
    return MakeBool(false);
}

}
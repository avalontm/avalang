#include "builtin.h"

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

ava_value_t MakeString(AvaVM* vm, const char* s, size_t len) {
    return ava_string_create(vm, s, len);
}

ava_value_t MakeList() {
    return ava_list_create(nullptr);
}

const char* GetDictEntryKey(AvaVM* vm, const ava_value_t& dict, size_t idx, size_t* out_len) {
    if (!vm || dict.type != AVA_DICT) return nullptr;
    void* entries = nullptr;
    size_t count = ava_dict_entries(vm, dict, &entries);
    if (idx >= count) return nullptr;
    
    auto* pairs = static_cast<ava_dict_pair_t*>(entries);
    if (out_len) *out_len = pairs[idx].key_len;
    return pairs[idx].key;
}

}

extern "C" {

ava_value_t builtin_dict_keys(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args || args[0].type != AVA_DICT) return MakeNil();
    
    ava_value_t result = MakeList();
    void* entries = nullptr;
    size_t count = ava_dict_entries(vm, args[0], &entries);
    auto* pairs = static_cast<ava_dict_pair_t*>(entries);
    
    for (size_t i = 0; i < count; i++) {
        ava_value_t key = MakeString(vm, pairs[i].key, pairs[i].key_len);
        ava_list_append(vm, result, key);
    }
    
    return result;
}

ava_value_t builtin_dict_values(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args || args[0].type != AVA_DICT) return MakeNil();
    
    ava_value_t result = MakeList();
    void* entries = nullptr;
    size_t count = ava_dict_entries(vm, args[0], &entries);
    auto* pairs = static_cast<ava_dict_pair_t*>(entries);
    
    for (size_t i = 0; i < count; i++) {
        ava_list_append(vm, result, pairs[i].value);
    }
    
    return result;
}

ava_value_t builtin_dict_items(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args || args[0].type != AVA_DICT) return MakeNil();
    
    ava_value_t result = MakeList();
    void* entries = nullptr;
    size_t count = ava_dict_entries(vm, args[0], &entries);
    auto* pairs = static_cast<ava_dict_pair_t*>(entries);
    
    for (size_t i = 0; i < count; i++) {
        ava_value_t pair = MakeList();
        ava_value_t key = MakeString(vm, pairs[i].key, pairs[i].key_len);
        ava_list_append(vm, pair, key);
        ava_list_append(vm, pair, pairs[i].value);
        ava_list_append(vm, result, pair);
    }
    
    return result;
}

ava_value_t builtin_dict_length(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args || args[0].type != AVA_DICT) return MakeNil();
    void* entries = nullptr;
    size_t count = ava_dict_entries(vm, args[0], &entries);
    return MakeNumber(static_cast<double>(count));
}

ava_value_t builtin_dict_containsKey(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2 || args[0].type != AVA_DICT) return MakeNil();
    
    if (args[1].type == AVA_STRING) {
        size_t key_len = 0;
        const char* key = ava_string_data(vm, args[1], &key_len);
        bool found = ava_dict_contains(vm, args[0], key, key_len);
        return MakeBool(found);
    }
    
    return MakeBool(false);
}

}
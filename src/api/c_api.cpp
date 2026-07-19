#include "ava.h"
#include "../vm/vm.h"
#include "../vm/value.h"
#include "../frontend/frontend.h"

#include <cstring>
#include <cstdlib>

using namespace ava;

struct AvaModule {
    std::shared_ptr<Proto> proto;
};

namespace {

char* DupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

} // namespace

extern "C" {

AVA_API AvaVM* ava_vm_create() {
    return reinterpret_cast<AvaVM*>(new VM());
}

AVA_API void ava_vm_destroy(AvaVM* vm) {
    delete reinterpret_cast<VM*>(vm);
}

AVA_API void ava_vm_register_native(AvaVM* vm, const char* name, AvaNativeFn fn, void* user_data) {
    reinterpret_cast<VM*>(vm)->RegisterNative(name, fn, user_data);
}

AVA_API AvaModule* ava_compile(AvaVM*, const char* source, const char* source_name, char** out_error) {
    try {
        auto proto = CompileSource(source, source_name ? source_name : "<script>");
        auto* module = new AvaModule();
        module->proto = proto;
        return module;
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        return nullptr;
    }
}

AVA_API void ava_module_destroy(AvaModule* module) {
    delete module;
}

AVA_API ava_value_t ava_run(AvaVM* vm, AvaModule* module, char** out_error) {
    try {
        Value result = reinterpret_cast<VM*>(vm)->Run(module->proto);
        return ToC(result);
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        return ToC(Value::Nil());
    }
}

AVA_API ava_value_t ava_call(AvaVM* vm, ava_value_t callable, const ava_value_t* args, size_t arg_count, char** out_error) {
    try {
        std::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));
        Value result = reinterpret_cast<VM*>(vm)->Call(FromC(callable), vargs);
        return ToC(result);
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        return ToC(Value::Nil());
    }
}

AVA_API ava_value_t ava_get_global(AvaVM* vm, const char* name) {
    return ToC(reinterpret_cast<VM*>(vm)->GetGlobal(name));
}

AVA_API void ava_set_global(AvaVM* vm, const char* name, ava_value_t value) {
    reinterpret_cast<VM*>(vm)->SetGlobal(name, FromC(value));
}

AVA_API ava_value_t ava_import(AvaVM* vm, const char* module_path, const char* alias, char** out_error) {
    try {
        Value result = reinterpret_cast<VM*>(vm)->DoImport(module_path, alias ? alias : "");
        return ToC(result);
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        return ToC(Value::Nil());
    }
}

AVA_API AvaCoroutine* ava_coroutine_create(AvaVM*, ava_value_t) {
    // TODO: implement once VM::Resume exists (see src/vm/coroutine.cpp).
    return nullptr;
}

AVA_API void ava_coroutine_destroy(AvaCoroutine*) { /* TODO */ }

AVA_API AvaCoStatus ava_coroutine_resume(AvaVM*, AvaCoroutine*, const ava_value_t*, size_t,
                                          ava_value_t*, size_t, size_t* out_count) {
    if (out_count) *out_count = 0;
    return AVA_CO_DEAD; // TODO: real implementation, see coroutine.cpp
}

AVA_API ava_value_t ava_string_create(AvaVM*, const char* utf8, size_t len) {
    auto* s = new StringObj(std::string(utf8, len));
    Value v; v.type = ValueType::String; v.obj = s;
    return ToC(v);
}

AVA_API const char* ava_string_data(AvaVM*, ava_value_t str, size_t* out_len) {
    Value v = FromC(str);
    auto* s = static_cast<StringObj*>(v.obj);
    if (out_len) *out_len = s->data.size();
    return s->data.c_str();
}

AVA_API ava_value_t ava_list_create(AvaVM*) {
    auto* l = new ListObj();
    Value v; v.type = ValueType::List; v.obj = l;
    return ToC(v);
}

AVA_API void ava_list_append(AvaVM*, ava_value_t list, ava_value_t item) {
    Value v = FromC(list);
    static_cast<ListObj*>(v.obj)->items.push_back(FromC(item));
}

AVA_API size_t ava_list_length(AvaVM*, ava_value_t list) {
    Value v = FromC(list);
    return static_cast<ListObj*>(v.obj)->items.size();
}

AVA_API ava_value_t ava_list_get(AvaVM*, ava_value_t list, size_t index) {
    Value v = FromC(list);
    auto& items = static_cast<ListObj*>(v.obj)->items;
    if (index >= items.size()) return ToC(Value::Nil());
    return ToC(items[index]);
}

AVA_API void ava_list_insert(AvaVM*, ava_value_t list, size_t index, ava_value_t item) {
    Value v = FromC(list);
    auto& items = static_cast<ListObj*>(v.obj)->items;
    if (index > items.size()) index = items.size();
    items.insert(items.begin() + index, FromC(item));
}

AVA_API void ava_list_remove(AvaVM*, ava_value_t list, size_t index) {
    Value v = FromC(list);
    auto& items = static_cast<ListObj*>(v.obj)->items;
    if (index >= items.size()) return;
    items.erase(items.begin() + index);
}

AVA_API ava_value_t ava_dict_create(AvaVM*) {
    auto* d = new DictObj();
    Value v; v.type = ValueType::Dict; v.obj = d;
    return ToC(v);
}

AVA_API void ava_dict_set(AvaVM*, ava_value_t dict, const char* key, ava_value_t value) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    auto it = d->index.find(key);
    if (it != d->index.end()) {
        d->entries[it->second].second = FromC(value);
    } else {
        d->index[key] = d->entries.size();
        d->entries.emplace_back(key, FromC(value));
    }
}

AVA_API ava_value_t ava_dict_get(AvaVM*, ava_value_t dict, const char* key) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    auto it = d->index.find(key);
    if (it == d->index.end()) return ToC(Value::Nil());
    return ToC(d->entries[it->second].second);
}

AVA_API size_t ava_dict_length(AvaVM*, ava_value_t dict) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    return d->entries.size();
}

AVA_API size_t ava_dict_entries(AvaVM*, ava_value_t dict, void** out_entries) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    if (out_entries) {
        *out_entries = d->entries.data();
    }
    return d->entries.size();
}

AVA_API int ava_dict_contains(AvaVM*, ava_value_t dict, const char* key, size_t key_len) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    std::string k(key, key_len);
    return d->index.find(k) != d->index.end() ? 1 : 0;
}

AVA_API void ava_value_retain(AvaVM*, ava_value_t value) {
    Retain(FromC(value));
}

AVA_API void ava_value_release(AvaVM*, ava_value_t value) {
    Release(FromC(value));
}

AVA_API void ava_string_free(char* s) {
    std::free(s);
}

} // extern "C"

#include "vm/vm.h"
#include "vm/value.h"
#include "frontend/frontend.h"
#include "builtins/builtin.h"
#include "builtins/builtin_natives.h"
#include "builtins/system_module.h"
#include "ui/builtins.h"
#include "compiler/proto_io.h"
#include "compiler/obfuscate.h"
#include "c_api_internal.h"

using namespace ava;

#include "avalang.h"

struct AvaModule {
    avastd::shared_ptr<Proto> proto;
};

static void ReportError(VM* raw_vm, const avastd::exception& e, bool has_pos,
                         int line, int column, const avastd::string& source,
                         char** out_error) {
    if (raw_vm) {
        raw_vm->last_error_line = has_pos ? line : 0;
        raw_vm->last_error_column = has_pos ? column : 0;
        if (has_pos) raw_vm->last_error_source = source;
        else raw_vm->last_error_source.clear();
    }
    if (out_error) *out_error = DupString(e.what());
}

extern "C" {

AVA_API AvaVM* ava_vm_create() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    RegisterSystemModule(*vm);
    ava::ui::RegisterUIBuiltins(reinterpret_cast<AvaVM*>(vm));
    return reinterpret_cast<AvaVM*>(vm);
}

AVA_API void ava_vm_destroy(AvaVM* vm) {
    delete reinterpret_cast<VM*>(vm);
}

AVA_API void ava_vm_set_current_dir(AvaVM* vm, const char* dir) {
    reinterpret_cast<VM*>(vm)->SetCurrentDir(dir ? dir : "");
}

AVA_API void ava_vm_add_search_path(AvaVM* vm, const char* path) {
    if (!path || !*path) return;
    reinterpret_cast<VM*>(vm)->GetModuleResolver().AddSearchPath(path);
}

AVA_API void ava_vm_set_stdlib_path(AvaVM* vm, const char* path) {
    reinterpret_cast<VM*>(vm)->GetModuleResolver().SetStdlibPath(path ? path : "");
}

AVA_API char* ava_vm_get_stdlib_path(AvaVM* vm) {
    return DupString(reinterpret_cast<VM*>(vm)->GetModuleResolver().GetStdlibPath());
}

AVA_API void ava_vm_register_native(AvaVM* vm, const char* name, AvaNativeFn fn, void* user_data) {
    reinterpret_cast<VM*>(vm)->RegisterNative(name, fn, user_data);
}

AVA_API void ava_vm_collect_garbage(AvaVM* vm, int64_t* out_collected) {
    GcSweepStats stats = reinterpret_cast<VM*>(vm)->CollectGarbage();
    if (out_collected) *out_collected = stats.collected;
}

AVA_API void ava_vm_set_print_callback(AvaVM* vm, AvaPrintFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetPrintSink([fn, user_data](const avastd::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetPrintSink(nullptr);
    }
}

AVA_API void ava_vm_set_input_callback(AvaVM* vm, AvaInputFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetInputSink([fn, user_data](const avastd::string& prompt) -> avastd::string {
            char* result = fn(prompt.data(), prompt.size(), user_data);
            if (!result) return avastd::string();
            avastd::string s(result);
            ava_free(result);
            return s;
        });
    } else {
        raw_vm->SetInputSink(nullptr);
    }
}

AVA_API void ava_vm_set_alert_callback(AvaVM* vm, AvaAlertFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetAlertSink([fn, user_data](const avastd::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetAlertSink(nullptr);
    }
}

AVA_API void ava_vm_set_navigate_callback(AvaVM* vm, AvaNavigateFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetNavigateSink([fn, user_data](const avastd::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetNavigateSink(nullptr);
    }
}

AVA_API AvaModule* ava_compile(AvaVM* vm, const char* source, const char* source_name, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
#if AVA_HAVE_EXCEPTIONS
    try {
        auto proto = CompileSource(source, source_name ? source_name : "<script>");
        auto* module = new AvaModule();
        module->proto = proto;
        return module;
    } catch (const AvaError& e) {
        ReportError(raw_vm, e, true, e.line, e.column, e.source, out_error);
        return nullptr;
    } catch (const avastd::exception& e) {
        ReportError(raw_vm, e, false, 0, 0, "", out_error);
        return nullptr;
    } catch (...) {
        ReportError(raw_vm, avastd::runtime_error("unknown error"), false, 0, 0, "", out_error);
        return nullptr;
    }
#else
    AvaModule* result = nullptr;
    AVA_TRY {
        auto proto = CompileSource(source, source_name ? source_name : "<script>");
        auto* module = new AvaModule();
        module->proto = proto;
        result = module;
    } AVA_CATCH(avastd::exception, e) {
        if (e.ava_type_tag() == 2) {
            const auto& ae = static_cast<const AvaError&>(e);
            ReportError(raw_vm, e, true, ae.line, ae.column, ae.source, out_error);
        } else {
            ReportError(raw_vm, e, false, 0, 0, "", out_error);
        }
        result = nullptr;
    }
    return result;
#endif
}

AVA_API void ava_module_destroy(AvaModule* module) {
    delete module;
}

AVA_API uint8_t* ava_module_serialize(AvaModule* module, const AvaModuleSerializeOptions* options,
                                       size_t* out_len, char** out_symbol_map) {
    if (out_len) *out_len = 0;
    if (out_symbol_map) *out_symbol_map = DupString("");
    if (!module || !module->proto) return nullptr;

    AvaModuleSerializeOptions opts{};
    if (options) opts = *options;

    if (opts.obfuscate) {
        ObfuscateOptions oopts;
        oopts.module_seed = opts.obfuscate_seed;
        oopts.strip_debug_lines = true;
        oopts.obfuscate_strings = opts.obfuscate_strings != 0;
        oopts.flatten_control_flow = opts.flatten_control_flow != 0;
        avastd::vector<SymbolMapEntry> symbol_map;
        ObfuscateProto(*module->proto, oopts, out_symbol_map ? &symbol_map : nullptr);
        if (out_symbol_map && !symbol_map.empty()) {
            ava_free(*out_symbol_map);
            *out_symbol_map = DupString(FormatSymbolMap(symbol_map));
        }
    }

    ProtoIoOptions pio;
    pio.strip_debug_info = opts.strip_debug_info != 0;
    avastd::vector<uint8_t> bytes = SerializeProto(*module->proto, pio);

    uint8_t* buf = static_cast<uint8_t*>(ava_alloc(bytes.size() > 0 ? bytes.size() : 1));
    if (!buf) return nullptr;
    if (!bytes.empty()) avastd::memcpy(buf, bytes.data(), bytes.size());
    if (out_len) *out_len = bytes.size();
    return buf;
}

AVA_API AvaModule* ava_module_deserialize(AvaVM* vm, const uint8_t* bytes, size_t len, char** out_error) {
    (void)vm;
    avastd::vector<uint8_t> buf(bytes, bytes + len);
    avastd::string err;
    auto proto = DeserializeProto(buf, &err);
    if (!proto) {
        if (out_error) *out_error = DupString(err);
        return nullptr;
    }
    auto* module = new AvaModule();
    module->proto = proto;
    return module;
}

AVA_API void ava_module_deobfuscate_strings(AvaModule* module, uint64_t seed) {
    if (!module || !module->proto) return;
    DeobfuscateStrings(*module->proto, seed);
}

AVA_API void ava_run(AvaVM* vm, AvaModule* module, ava_value_t* out_result, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
#if AVA_HAVE_EXCEPTIONS
    try {
        Value result = raw_vm->Run(module->proto);
        // ava_run hands out_result to the host as a retained reference
        // (see the "Ref-counting" contract on ava_value_retain/release
        // in avalang.h): the host is expected to eventually call
        // ava_value_release() on it. Without this Retain(), `result`'s
        // own destructor (RAII, ~Value() -> Release()) fires when this
        // function returns and can drop the object's refcount to 0 --
        // freeing it before/as the host even receives the handle. That
        // was a use-after-free: the host's out_result pointed at freed
        // memory the moment control returned to it.
        Retain(result);
        if (out_result) *out_result = ToC(result);
    } catch (const AvaError& e) {
        ReportError(raw_vm, e, true, e.line, e.column, e.source, out_error);
        if (out_result) out_result->type = AVA_NIL;
    } catch (const avastd::exception& e) {
        ReportError(raw_vm, e, false, 0, 0, "", out_error);
        if (out_result) out_result->type = AVA_NIL;
    }
#else
    AVA_TRY {
        Value result = raw_vm->Run(module->proto);
        Retain(result);  // ver comentario en la rama AVA_HAVE_EXCEPTIONS arriba
        if (out_result) *out_result = ToC(result);
    } AVA_CATCH(avastd::exception, e) {
        if (e.ava_type_tag() == 2) {
            const auto& ae = static_cast<const AvaError&>(e);
            ReportError(raw_vm, e, true, ae.line, ae.column, ae.source, out_error);
        } else {
            ReportError(raw_vm, e, false, 0, 0, "", out_error);
        }
        if (out_result) out_result->type = AVA_NIL;
    }
#endif
}

AVA_API void ava_call(AvaVM* vm, ava_value_t callable, const ava_value_t* args, size_t arg_count, ava_value_t* out_result, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
#if AVA_HAVE_EXCEPTIONS
    try {
        avastd::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));
        Value result = raw_vm->Call(FromC(callable), vargs);
        Retain(result);  // ver comentario en ava_run sobre por que hace falta
        if (out_result) *out_result = ToC(result);
    } catch (const AvaError& e) {
        ReportError(raw_vm, e, true, e.line, e.column, e.source, out_error);
        if (out_result) out_result->type = AVA_NIL;
    } catch (const avastd::exception& e) {
        ReportError(raw_vm, e, false, 0, 0, "", out_error);
        if (out_result) out_result->type = AVA_NIL;
    }
#else
    AVA_TRY {
        avastd::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));
        Value result = raw_vm->Call(FromC(callable), vargs);
        Retain(result);  // ver comentario en ava_run sobre por que hace falta
        if (out_result) *out_result = ToC(result);
    } AVA_CATCH(avastd::exception, e) {
        if (e.ava_type_tag() == 2) {
            const auto& ae = static_cast<const AvaError&>(e);
            ReportError(raw_vm, e, true, ae.line, ae.column, ae.source, out_error);
        } else {
            ReportError(raw_vm, e, false, 0, 0, "", out_error);
        }
        if (out_result) out_result->type = AVA_NIL;
    }
#endif
}

AVA_API ava_value_t ava_get_global(AvaVM* vm, const char* name) {
    return ToC(reinterpret_cast<VM*>(vm)->GetGlobal(name));
}

AVA_API void ava_set_global(AvaVM* vm, const char* name, ava_value_t value) {
    reinterpret_cast<VM*>(vm)->SetGlobal(name, FromC(value));
}

AVA_API ava_value_t ava_import(AvaVM* vm, const char* module_path, const char* alias, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
#if AVA_HAVE_EXCEPTIONS
    try {
        Value result = raw_vm->DoImport(module_path, alias ? alias : "");
        Retain(result);  // ver comentario en ava_run sobre por que hace falta
        return ToC(result);
    } catch (const AvaError& e) {
        ReportError(raw_vm, e, true, e.line, e.column, e.source, out_error);
        return ToC(Value::Nil());
    } catch (const avastd::exception& e) {
        ReportError(raw_vm, e, false, 0, 0, "", out_error);
        return ToC(Value::Nil());
    }
#else
    ava_value_t result = ToC(Value::Nil());
    AVA_TRY {
        Value r = raw_vm->DoImport(module_path, alias ? alias : "");
        Retain(r);  // ver comentario en ava_run sobre por que hace falta
        result = ToC(r);
    } AVA_CATCH(avastd::exception, e) {
        if (e.ava_type_tag() == 2) {
            const auto& ae = static_cast<const AvaError&>(e);
            ReportError(raw_vm, e, true, ae.line, ae.column, ae.source, out_error);
        } else {
            ReportError(raw_vm, e, false, 0, 0, "", out_error);
        }
        result = ToC(Value::Nil());
    }
    return result;
#endif
}

AVA_API AvaCoroutine* ava_coroutine_create(AvaVM* vm, ava_value_t func) {
#if AVA_HAVE_EXCEPTIONS
    try {
        auto* co = reinterpret_cast<VM*>(vm)->CreateCoroutine(FromC(func));
        return reinterpret_cast<AvaCoroutine*>(co);
    } catch (...) {
        return nullptr;
    }
#else
    AvaCoroutine* result = nullptr;
    AVA_TRY {
        auto* co = reinterpret_cast<VM*>(vm)->CreateCoroutine(FromC(func));
        result = reinterpret_cast<AvaCoroutine*>(co);
    } AVA_CATCH(avastd::exception, e) {
        (void)e;
        result = nullptr;
    }
    return result;
#endif
}

AVA_API void ava_coroutine_destroy(AvaCoroutine* co) {
    delete reinterpret_cast<Coroutine*>(co);
}

AVA_API AvaCoStatus ava_coroutine_resume(AvaVM* vm, AvaCoroutine* co,
                                          const ava_value_t* args, size_t arg_count,
                                          ava_value_t* out_values, size_t max_out,
                                          size_t* out_count) {
    auto* coroutine = reinterpret_cast<Coroutine*>(co);
    if (coroutine->status == CoStatus::Dead) {
        if (out_count) *out_count = 0;
        return AVA_CO_DEAD;
    }
    if (coroutine->status == CoStatus::Running) {
        if (out_count) *out_count = 0;
        return AVA_CO_RUNNING;
    }

    AvaCoStatus status = AVA_CO_DEAD;
#if AVA_HAVE_EXCEPTIONS
    try {
#else
    AVA_TRY {
#endif
        avastd::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));

        auto result = reinterpret_cast<VM*>(vm)->Call(Value::Coroutine(coroutine), vargs);

        size_t yielded_count = 0;
        if (result.type == ValueType::List) {
            auto* list = static_cast<ListObj*>(result.obj);
            for (size_t i = 0; i < list->items.size() && i < max_out; ++i) {
                out_values[i] = ToC(list->items[i]);
                yielded_count++;
            }
        }

        if (out_count) *out_count = yielded_count;
        status = (coroutine->status == CoStatus::Dead) ? AVA_CO_DEAD : AVA_CO_SUSPENDED;
#if AVA_HAVE_EXCEPTIONS
    } catch (...) {
        if (out_count) *out_count = 0;
        status = AVA_CO_DEAD;
    }
#else
    } AVA_CATCH(avastd::exception, e) {
        (void)e;
        if (out_count) *out_count = 0;
        status = AVA_CO_DEAD;
    }
#endif
    return status;
}

AVA_API AvaCoStatus ava_coroutine_status(AvaVM*, AvaCoroutine* co) {
    auto* coroutine = reinterpret_cast<Coroutine*>(co);
    switch (coroutine->status) {
        case CoStatus::Suspended: return AVA_CO_SUSPENDED;
        case CoStatus::Running:   return AVA_CO_RUNNING;
        case CoStatus::Dead:       return AVA_CO_DEAD;
    }
    return AVA_CO_DEAD;
}

AVA_API ava_value_t ava_string_create(AvaVM*, const char* utf8, size_t len) {
    auto* s = new StringObj(avastd::string(utf8, len));
    Value v; v.type = ValueType::String; v.obj = s;
    Retain(v);  // ver comentario en ava_run sobre por que hace falta
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
    Retain(v);  // ver comentario en ava_run sobre por que hace falta
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

AVA_API void ava_list_set(AvaVM*, ava_value_t list, size_t index, ava_value_t value) {
    Value v = FromC(list);
    auto& items = static_cast<ListObj*>(v.obj)->items;
    if (index >= items.size()) return;
    items[index] = FromC(value);
}

AVA_API ava_value_t ava_dict_create(AvaVM*) {
    auto* d = new DictObj();
    Value v; v.type = ValueType::Dict; v.obj = d;
    Retain(v);  // ver comentario en ava_run sobre por que hace falta
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

    d->c_entries_cache.clear();
    d->c_entries_cache.reserve(d->entries.size());
    for (auto& [key, value] : d->entries) {
        ava_dict_pair_t pair;
        pair.key = key.c_str();
        pair.key_len = key.size();
        pair.value = ToC(value);
        d->c_entries_cache.push_back(pair);
    }

    if (out_entries) {
        *out_entries = d->c_entries_cache.data();
    }
    return d->c_entries_cache.size();
}

AVA_API int ava_dict_contains(AvaVM*, ava_value_t dict, const char* key, size_t key_len) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    avastd::string k(key, key_len);
    return d->index.find(k) != d->index.end() ? 1 : 0;
}

AVA_API void ava_value_retain(AvaVM*, ava_value_t value) {
    Retain(FromC(value));
}

AVA_API void ava_value_release(AvaVM*, ava_value_t value) {
    Release(FromC(value));
}

AVA_API void ava_string_free(char* s) {
    ava_free(s);
}

AVA_API int ava_last_error_line(AvaVM* vm) {
    return vm ? reinterpret_cast<VM*>(vm)->last_error_line : 0;
}

AVA_API int ava_last_error_column(AvaVM* vm) {
    return vm ? reinterpret_cast<VM*>(vm)->last_error_column : 0;
}

AVA_API char* ava_last_error_source(AvaVM* vm) {
    if (!vm) return DupString("");
    return DupString(reinterpret_cast<VM*>(vm)->last_error_source);
}

}

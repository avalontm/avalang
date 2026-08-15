#include "vm/vm.h"
#include "vm/value.h"
#include "frontend/frontend.h"
#include "components/IComponent.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "parser/AvauiParser.h"
#include "parser/AvauiWriter.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"
#include "builtins/builtin.h"
#include "builtins/builtin_natives.h"
#include "ui/builtins.h"
#include "compiler/proto_io.h"
#include "compiler/obfuscate.h"

#include <cstring>
#include <cstdlib>
#include <sstream>


using namespace ava;

#include "avalang.h"

struct AvaModule {
    std::shared_ptr<Proto> proto;
};

static char* DupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

static int LayoutNameToId(const std::string& type) {
    if (type == "Column") return 1;
    if (type == "Row") return 2;
    if (type == "Stack") return 3;
    if (type == "Grid") return 4;
    if (type == "Flex") return 5;
    return 0;
}

static avalang::ui::PropertyValue ToPropertyValue(ava_value_t v) {
    switch (v.type) {
        case AVA_BOOL:   return avalang::ui::PropertyValue(v.as.b != 0);
        case AVA_NUMBER: return avalang::ui::PropertyValue(v.as.n);
        case AVA_STRING: {
            Value sv = FromC(v);
            if (sv.type == ValueType::String && sv.obj) {
                return avalang::ui::PropertyValue(static_cast<StringObj*>(sv.obj)->data);
            }
            return avalang::ui::PropertyValue(std::string());
        }
        default: {
            Value sv = FromC(v);
            if (sv.type == ValueType::String && sv.obj) {
                return avalang::ui::PropertyValue(static_cast<StringObj*>(sv.obj)->data);
            }
            return avalang::ui::PropertyValue(std::string());
        }
    }
}

static ava_value_t ToAvaValue(const avalang::ui::PropertyValue& pv) {
    switch (pv.Type()) {
        case avalang::ui::PropertyType::Bool:
            return ava_value_t{AVA_BOOL, {.b = pv.AsBool() ? 1 : 0}};
        case avalang::ui::PropertyType::Number:
            return ava_value_t{AVA_NUMBER, {.n = pv.AsNumber()}};
        case avalang::ui::PropertyType::String: {
            const std::string& s = pv.AsString();
            auto* so = new StringObj(s);
            Value v; v.type = ValueType::String; v.obj = so;
            return ToC(v);
        }
        default:
            return ava_value_t{AVA_NIL, {0}};
    }
}

static std::string StateMapToJson(const std::unordered_map<std::string, std::string>& state) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : state) {
        if (!first) oss << ", ";
        oss << "\"" << k << "\": \"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

static std::string ImportsToJson(const std::vector<std::string>& imports) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < imports.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << imports[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

static std::string RoutesToJson(const std::vector<avalang::ui::parser::RouteDeclaration>& routes) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < routes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "{\"template\": \"" << routes[i].route_template << "\", \"parameters\": [";
        for (size_t j = 0; j < routes[i].parameters.size(); ++j) {
            if (j > 0) oss << ", ";
            oss << "{\"name\": \"" << routes[i].parameters[j].name << "\"";
            oss << ", \"optional\": " << (routes[i].parameters[j].kind == avalang::ui::parser::RouteParameterKind::Optional ? "true" : "false");
            if (!routes[i].parameters[j].constraint.empty()) {
                oss << ", \"constraint\": \"" << routes[i].parameters[j].constraint << "\"";
            }
            oss << "}";
        }
        oss << "]}";
    }
    oss << "]";
    return oss.str();
}

static void ComponentToJson(std::ostream& os, avalang::ui::IComponent* comp, int indent) {
    if (!comp) return;
    std::string pad(indent * 2, ' ');
    os << pad << "{\n";
    os << pad << "  \"type\": \"" << comp->TypeName() << "\"";
    const auto* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()) {
        os << ",\n" << pad << "  \"id\": \"" << idProp->AsString() << "\"";
    }
    int layout = 0;
    const auto* layoutProp = comp->GetProperty("__layout");
    if (layoutProp && layoutProp->Type() == avalang::ui::PropertyType::Number) {
        layout = static_cast<int>(layoutProp->AsNumber());
    } else {
        layout = LayoutNameToId(comp->TypeName());
    }
    os << ",\n" << pad << "  \"layout\": " << layout;
    auto names = comp->PropertyNames();
    bool has_props = false;
    for (const auto& name : names) {
        if (name == "id" || name == "__layout") continue;
        has_props = true;
        break;
    }
    if (has_props) {
        os << ",\n" << pad << "  \"properties\": {";
        bool first = true;
        for (const auto& name : names) {
            if (name == "id" || name == "__layout") continue;
            const auto* pv = comp->GetProperty(name);
            if (!pv) continue;
            if (!first) os << ", ";
            os << "\"" << name << "\": ";
            switch (pv->Type()) {
                case avalang::ui::PropertyType::String: {
                    os << "\"";
                    for (char c : pv->AsString()) {
                        if (c == '"' || c == '\\') os << '\\';
                        os << c;
                    }
                    os << "\"";
                    break;
                }
                case avalang::ui::PropertyType::Number:
                    os << pv->AsNumber();
                    break;
                case avalang::ui::PropertyType::Bool:
                    os << (pv->AsBool() ? "true" : "false");
                    break;
                default:
                    os << "null";
                    break;
            }
            first = false;
        }
        os << "}";
    }
    auto children = comp->Children();
    if (!children.empty()) {
        os << ",\n" << pad << "  \"children\": [\n";
        for (size_t i = 0; i < children.size(); ++i) {
            ComponentToJson(os, children[i], indent + 2);
            if (i < children.size() - 1) os << ",";
            os << "\n";
        }
        os << pad << "  ]";
    }
    os << "\n" << pad << "}";
}

extern "C" {

AVA_API AvaVM* ava_vm_create() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
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

AVA_API void ava_vm_set_print_callback(AvaVM* vm, AvaPrintFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetPrintSink([fn, user_data](const std::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetPrintSink(nullptr);
    }
}

AVA_API void ava_vm_set_input_callback(AvaVM* vm, AvaInputFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetInputSink([fn, user_data](const std::string& prompt) -> std::string {
            char* result = fn(prompt.data(), prompt.size(), user_data);
            if (!result) return std::string();
            std::string s(result);
            std::free(result);
            return s;
        });
    } else {
        raw_vm->SetInputSink(nullptr);
    }
}

AVA_API void ava_vm_set_alert_callback(AvaVM* vm, AvaAlertFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetAlertSink([fn, user_data](const std::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetAlertSink(nullptr);
    }
}

AVA_API void ava_vm_set_navigate_callback(AvaVM* vm, AvaNavigateFn fn, void* user_data) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    if (fn) {
        raw_vm->SetNavigateSink([fn, user_data](const std::string& text) {
            fn(text.data(), text.size(), user_data);
        });
    } else {
        raw_vm->SetNavigateSink(nullptr);
    }
}

AVA_API AvaModule* ava_compile(AvaVM* vm, const char* source, const char* source_name, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    try {
        auto proto = CompileSource(source, source_name ? source_name : "<script>");
        auto* module = new AvaModule();
        module->proto = proto;
        return module;
    } catch (const AvaError& e) {
        if (raw_vm) { raw_vm->last_error_line = e.line; raw_vm->last_error_column = e.column; raw_vm->last_error_source = e.source; }
        if (out_error) *out_error = DupString(e.what());
        return nullptr;
    } catch (const std::exception& e) {
        if (raw_vm) { raw_vm->last_error_line = 0; raw_vm->last_error_column = 0; raw_vm->last_error_source.clear(); }
        if (out_error) *out_error = DupString(e.what());
        return nullptr;
    } catch (...) {
        if (raw_vm) { raw_vm->last_error_line = 0; raw_vm->last_error_column = 0; raw_vm->last_error_source.clear(); }
        if (out_error) *out_error = DupString("unknown error");
        return nullptr;
    }
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
        std::vector<SymbolMapEntry> symbol_map;
        ObfuscateProto(*module->proto, oopts, out_symbol_map ? &symbol_map : nullptr);
        if (out_symbol_map && !symbol_map.empty()) {
            std::free(*out_symbol_map);
            *out_symbol_map = DupString(FormatSymbolMap(symbol_map));
        }
    }

    ProtoIoOptions pio;
    pio.strip_debug_info = opts.strip_debug_info != 0;
    std::vector<uint8_t> bytes = SerializeProto(*module->proto, pio);

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(bytes.size() > 0 ? bytes.size() : 1));
    if (!buf) return nullptr;
    if (!bytes.empty()) std::memcpy(buf, bytes.data(), bytes.size());
    if (out_len) *out_len = bytes.size();
    return buf;
}

AVA_API AvaModule* ava_module_deserialize(AvaVM* vm, const uint8_t* bytes, size_t len, char** out_error) {
    (void)vm;
    std::vector<uint8_t> buf(bytes, bytes + len);
    std::string err;
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
    try {
        Value result = raw_vm->Run(module->proto);
        if (out_result) *out_result = ToC(result);
    } catch (const AvaError& e) {
        raw_vm->last_error_line = e.line;
        raw_vm->last_error_column = e.column;
        raw_vm->last_error_source = e.source;
        if (out_error) *out_error = DupString(e.what());
        if (out_result) out_result->type = AVA_NIL;
    } catch (const std::exception& e) {
        raw_vm->last_error_line = 0;
        raw_vm->last_error_column = 0;
        raw_vm->last_error_source.clear();
        if (out_error) *out_error = DupString(e.what());
        if (out_result) out_result->type = AVA_NIL;
    }
}

AVA_API void ava_call(AvaVM* vm, ava_value_t callable, const ava_value_t* args, size_t arg_count, ava_value_t* out_result, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    try {
        std::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));
        Value result = raw_vm->Call(FromC(callable), vargs);
        if (out_result) *out_result = ToC(result);
    } catch (const AvaError& e) {
        raw_vm->last_error_line = e.line;
        raw_vm->last_error_column = e.column;
        raw_vm->last_error_source = e.source;
        if (out_error) *out_error = DupString(e.what());
        if (out_result) out_result->type = AVA_NIL;
    } catch (const std::exception& e) {
        raw_vm->last_error_line = 0;
        raw_vm->last_error_column = 0;
        raw_vm->last_error_source.clear();
        if (out_error) *out_error = DupString(e.what());
        if (out_result) out_result->type = AVA_NIL;
    }
}

AVA_API ava_value_t ava_get_global(AvaVM* vm, const char* name) {
    return ToC(reinterpret_cast<VM*>(vm)->GetGlobal(name));
}

AVA_API void ava_set_global(AvaVM* vm, const char* name, ava_value_t value) {
    reinterpret_cast<VM*>(vm)->SetGlobal(name, FromC(value));
}

AVA_API ava_value_t ava_import(AvaVM* vm, const char* module_path, const char* alias, char** out_error) {
    auto* raw_vm = reinterpret_cast<VM*>(vm);
    try {
        Value result = raw_vm->DoImport(module_path, alias ? alias : "");
        return ToC(result);
    } catch (const AvaError& e) {
        raw_vm->last_error_line = e.line;
        raw_vm->last_error_column = e.column;
        raw_vm->last_error_source = e.source;
        if (out_error) *out_error = DupString(e.what());
        return ToC(Value::Nil());
    } catch (const std::exception& e) {
        raw_vm->last_error_line = 0;
        raw_vm->last_error_column = 0;
        raw_vm->last_error_source.clear();
        if (out_error) *out_error = DupString(e.what());
        return ToC(Value::Nil());
    }
}

AVA_API AvaCoroutine* ava_coroutine_create(AvaVM* vm, ava_value_t func) {
    try {
        auto* co = reinterpret_cast<VM*>(vm)->CreateCoroutine(FromC(func));
        return reinterpret_cast<AvaCoroutine*>(co);
    } catch (...) {
        return nullptr;
    }
}

AVA_API void ava_coroutine_destroy(AvaCoroutine* co) {
    delete reinterpret_cast<Coroutine*>(co);
}

AVA_API AvaCoStatus ava_coroutine_resume(AvaVM* vm, AvaCoroutine* co,
                                          const ava_value_t* args, size_t arg_count,
                                          ava_value_t* out_values, size_t max_out,
                                          size_t* out_count) {
    try {
        auto* coroutine = reinterpret_cast<Coroutine*>(co);
        if (coroutine->status == CoStatus::Dead) {
            if (out_count) *out_count = 0;
            return AVA_CO_DEAD;
        }
        if (coroutine->status == CoStatus::Running) {
            if (out_count) *out_count = 0;
            return AVA_CO_RUNNING;
        }

        std::vector<Value> vargs;
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

        if (coroutine->status == CoStatus::Dead) {
            return AVA_CO_DEAD;
        }
        return AVA_CO_SUSPENDED;
    } catch (...) {
        if (out_count) *out_count = 0;
        return AVA_CO_DEAD;
    }
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

AVA_API void ava_list_set(AvaVM*, ava_value_t list, size_t index, ava_value_t value) {
    Value v = FromC(list);
    auto& items = static_cast<ListObj*>(v.obj)->items;
    if (index >= items.size()) return;
    items[index] = FromC(value);
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

struct AvaComponent {
    avalang::ui::IComponent* comp;
    std::unique_ptr<avalang::ui::ComponentTree> owned_tree;

    explicit AvaComponent(avalang::ui::IComponent* c, std::unique_ptr<avalang::ui::ComponentTree> t = nullptr)
        : comp(c), owned_tree(std::move(t)) {}
};

struct AvaComponentTree {
    std::unique_ptr<avalang::ui::ComponentTree> tree;
    AvaComponentTree() : tree(avalang::ui::ComponentTree::Create()) {}
};

AVA_API AvaComponentTree* ava_ui_create_tree(void) {
    return new AvaComponentTree();
}

AVA_API void ava_ui_destroy_tree(AvaComponentTree* tree) {
    delete tree;
}

AVA_API AvaComponent* ava_ui_create_component(const char* type) {
    auto tree = avalang::ui::ComponentTree::Create();
    auto* comp = tree->CreateComponent(type ? type : "");
    return new AvaComponent(comp, std::move(tree));
}

AVA_API void ava_ui_destroy_component(AvaComponent* component) {
    delete component;
}

AVA_API void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value) {
    if (!comp || !key) return;
    comp->comp->SetProperty(key, ToPropertyValue(value));
}

AVA_API int ava_ui_has_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return 0;
    return comp->comp->HasProperty(key) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return ava_value_t{AVA_NIL, {0}};
    const auto* pv = comp->comp->GetProperty(key);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(*pv);
}

AVA_API void ava_ui_remove_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return;
    comp->comp->RemoveProperty(key);
}

AVA_API size_t ava_ui_property_count(AvaComponent* comp) {
    if (!comp) return 0;
    return comp->comp->PropertyNames().size();
}

AVA_API const char* ava_ui_property_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    auto names = comp->comp->PropertyNames();
    if (index >= names.size()) return nullptr;
    static thread_local std::string key_str;
    key_str = names[index];
    return key_str.c_str();
}

AVA_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->AddChild(child->comp);
}

AVA_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->RemoveChild(child->comp);
}

AVA_API size_t ava_ui_child_count(AvaComponent* parent) {
    if (!parent) return 0;
    return parent->comp->Children().size();
}

AVA_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index) {
    if (!parent) return nullptr;
    auto children = parent->comp->Children();
    if (index >= children.size()) return nullptr;
    return new AvaComponent(children[index]);
}

AVA_API void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t callback) {
    if (!comp || !event) return;
    comp->comp->SetProperty(event, ToPropertyValue(callback));
}

AVA_API int ava_ui_has_event(AvaComponent* comp, const char* event) {
    if (!comp || !event) return 0;
    if (!comp->comp->HasProperty(event)) return 0;
    return avalang::ui::IsEventPropertyName(event) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_event(AvaComponent* comp, const char* event) {
    if (!comp || !event) return ava_value_t{AVA_NIL, {0}};
    const auto* pv = comp->comp->GetProperty(event);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(*pv);
}

AVA_API size_t ava_ui_event_count(AvaComponent* comp) {
    if (!comp) return 0;
    size_t count = 0;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (avalang::ui::IsEventPropertyName(name)) ++count;
    }
    return count;
}

AVA_API const char* ava_ui_event_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    std::vector<std::string> event_names;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (avalang::ui::IsEventPropertyName(name)) {
            event_names.push_back(name);
        }
    }
    if (index >= event_names.size()) return nullptr;
    static thread_local std::string key_str;
    key_str = event_names[index];
    return key_str.c_str();
}

AVA_API void ava_ui_set_id(AvaComponent* comp, const char* id) {
    if (!comp) return;
    comp->comp->SetProperty("id", avalang::ui::PropertyValue(std::string(id ? id : "")));
}

AVA_API const char* ava_ui_get_id(AvaComponent* comp) {
    if (!comp) return nullptr;
    const auto* pv = comp->comp->GetProperty("id");
    if (!pv || pv->Type() != avalang::ui::PropertyType::String) return "";
    static thread_local std::string id_str;
    id_str = pv->AsString();
    return id_str.c_str();
}

AVA_API void ava_ui_set_layout(AvaComponent* comp, int layout) {
    if (!comp) return;
    comp->comp->SetProperty("__layout", avalang::ui::PropertyValue(static_cast<double>(layout)));
}

AVA_API int ava_ui_get_layout(AvaComponent* comp) {
    if (!comp) return 0;
    const auto* pv = comp->comp->GetProperty("__layout");
    if (!pv || pv->Type() != avalang::ui::PropertyType::Number) return 0;
    return static_cast<int>(pv->AsNumber());
}

AVA_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root) {
    if (!tree || !root) return;
    tree->tree->SetRoot(root->comp);
}

AVA_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree) {
    if (!tree) return nullptr;
    auto* root = tree->tree->Root();
    if (!root) return nullptr;
    return new AvaComponent(root);
}

AVA_API const char* ava_ui_get_component_type(AvaComponent* comp) {
    if (!comp) return nullptr;
    static thread_local std::string type_str;
    type_str = comp->comp->TypeName();
    return type_str.c_str();
}

AVA_API const char* ava_ui_tree_to_json(AvaComponentTree* tree) {
    if (!tree) return "";
    static thread_local std::ostringstream oss;
    oss.str(""); oss.clear();
    auto* root = tree->tree->Root();
    if (root) {
        ComponentToJson(oss, root, 0);
    }
    static thread_local std::string result;
    result = oss.str();
    return result.c_str();
}

AVA_API void ava_ui_json_free(char* json) {
    (void)json;
}

AVA_API AvaComponentTree* ava_ui_parse_avaui_text(
    const char* text,
    char** out_state_json,
    char** out_imports_json,
    char** out_methods_text,
    char** out_error,
    char** out_extends,
    char** out_routes_json
) {
    auto* result = new AvaComponentTree();

    try {
        auto parsed = avalang::ui::parser::AvauiParser::Parse(text ? text : "");
        result->tree = std::move(parsed.tree);

        if (out_state_json) *out_state_json = DupString(StateMapToJson(parsed.state));
        if (out_imports_json) *out_imports_json = DupString(ImportsToJson(parsed.imports));
        if (out_methods_text) *out_methods_text = DupString(parsed.code);
        if (out_error) *out_error = DupString("");
        if (out_extends) *out_extends = DupString(parsed.extends);
        if (out_routes_json) *out_routes_json = DupString(RoutesToJson(parsed.routes));

        return result;
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        return result;
    } catch (...) {
        if (out_error) *out_error = DupString("unknown parse error");
        return result;
    }
}

AVA_API char* ava_ui_write_avaui_text(
    AvaComponentTree* tree,
    const char* state_json,
    const char* imports_json,
    const char* methods_text,
    const char* extends,
    const char* routes_json
) {
    if (!tree) return DupString("");
    auto* root = tree->tree->Root();
    if (!root) return DupString("");

    avalang::ui::parser::AvauiWriteOptions opts;
    opts.code_behind = methods_text ? methods_text : "";
    opts.extends = extends ? extends : "";

    if (state_json && *state_json) {
        std::istringstream ss(state_json);
        std::string line;
        while (std::getline(ss, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (!key.empty() && !val.empty()) {
                opts.initial_state.push_back({key, val});
            }
        }
    }

    if (imports_json && *imports_json) {
        std::istringstream ss(imports_json);
        std::string line;
        while (std::getline(ss, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t[]\""));
            trimmed.erase(trimmed.find_last_not_of(" \t\"]") + 1);
            if (!trimmed.empty()) {
                opts.imports.push_back(trimmed);
            }
        }
    }

    if (routes_json && *routes_json) {
        std::istringstream ss(routes_json);
        std::string line;
        while (std::getline(ss, line)) {
            auto tplStart = line.find("\"template\"");
            if (tplStart == std::string::npos) continue;
            auto colon = line.find(":", tplStart);
            if (colon == std::string::npos) continue;
            auto q1 = line.find("\"", colon);
            if (q1 == std::string::npos) continue;
            auto q2 = line.find("\"", q1 + 1);
            if (q2 == std::string::npos) continue;
            std::string tpl = line.substr(q1 + 1, q2 - q1 - 1);
            if (!tpl.empty()) {
                opts.routes.push_back({tpl});
            }
        }
    }

    return DupString(avalang::ui::parser::WriteAvaui(root, opts));
}

AVA_API void ava_ui_text_free(char* text) {
    std::free(text);
}

}
#include "avalang.h"
#include "vm/vm.h"
#include "vm/value.h"
#include "frontend/frontend.h"
#include "ui/component.h"
#include "ui/tree.h"
#include "builtins/builtin.h"
#include "builtins/builtin_natives.h"

#include <cstring>
#include <cstdlib>
#include <sstream>

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
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    return reinterpret_cast<AvaVM*>(vm);
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
    } catch (...) {
        if (out_error) *out_error = DupString("unknown error");
        return nullptr;
    }
}

AVA_API void ava_module_destroy(AvaModule* module) {
    delete module;
}

AVA_API void ava_run(AvaVM* vm, AvaModule* module, ava_value_t* out_result, char** out_error) {
    try {
        Value result = reinterpret_cast<VM*>(vm)->Run(module->proto);
        if (out_result) *out_result = ToC(result);
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
        if (out_result) out_result->type = AVA_NIL;
    }
}

AVA_API void ava_call(AvaVM* vm, ava_value_t callable, const ava_value_t* args, size_t arg_count, ava_value_t* out_result, char** out_error) {
    try {
        std::vector<Value> vargs;
        vargs.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) vargs.push_back(FromC(args[i]));
        Value result = reinterpret_cast<VM*>(vm)->Call(FromC(callable), vargs);
        if (out_result) *out_result = ToC(result);
    } catch (const std::exception& e) {
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
    try {
        Value result = reinterpret_cast<VM*>(vm)->DoImport(module_path, alias ? alias : "");
        return ToC(result);
    } catch (const std::exception& e) {
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

struct AvaComponent {
    std::shared_ptr<ava::ui::Component> comp;
    explicit AvaComponent(const std::string& type) : comp(std::make_shared<ava::ui::Component>(type)) {}
};

struct AvaComponentTree {
    std::shared_ptr<ava::ui::ComponentTree> tree;
    AvaComponentTree() : tree(std::make_shared<ava::ui::ComponentTree>()) {}
};

AVA_API AvaComponentTree* ava_ui_create_tree(void) {
    return new AvaComponentTree();
}

AVA_API void ava_ui_destroy_tree(AvaComponentTree* tree) {
    delete tree;
}

AVA_API AvaComponent* ava_ui_create_component(const char* type) {
    return new AvaComponent(type);
}

AVA_API void ava_ui_destroy_component(AvaComponent* component) {
    delete component;
}

AVA_API void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value) {
    if (!comp) return;
    comp->comp->SetProperty(key, FromC(value));
}

AVA_API int ava_ui_has_property(AvaComponent* comp, const char* key) {
    if (!comp) return 0;
    return comp->comp->HasProperty(key) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_property(AvaComponent* comp, const char* key) {
    if (!comp) return ava_value_t{AVA_NIL, {0}};
    return ToC(comp->comp->GetProperty(key));
}

AVA_API void ava_ui_remove_property(AvaComponent* comp, const char* key) {
    if (!comp) return;
    comp->comp->RemoveProperty(key);
}

AVA_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->AddChild(child->comp);
}

AVA_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->RemoveChild(child->comp.get());
}

AVA_API size_t ava_ui_child_count(AvaComponent* parent) {
    if (!parent) return 0;
    return parent->comp->GetChildren().size();
}

AVA_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index) {
    if (!parent) return nullptr;
    const auto& children = parent->comp->GetChildren();
    if (index >= children.size()) return nullptr;
    auto* wrapper = new AvaComponent(children[index]->GetType());
    wrapper->comp = children[index];
    return wrapper;
}

AVA_API void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t callback) {
    if (!comp) return;
    comp->comp->SetEvent(event, FromC(callback));
}

AVA_API int ava_ui_has_event(AvaComponent* comp, const char* event) {
    if (!comp) return 0;
    return comp->comp->HasEvent(event) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_event(AvaComponent* comp, const char* event) {
    if (!comp) return ava_value_t{AVA_NIL, {0}};
    return ToC(comp->comp->GetEvent(event));
}

AVA_API void ava_ui_set_id(AvaComponent* comp, const char* id) {
    if (!comp) return;
    comp->comp->SetId(id);
}

AVA_API const char* ava_ui_get_id(AvaComponent* comp) {
    if (!comp) return nullptr;
    static thread_local std::string id_str;
    id_str = comp->comp->GetId();
    return id_str.c_str();
}

AVA_API void ava_ui_set_layout(AvaComponent* comp, int layout) {
    if (!comp) return;
    comp->comp->SetLayout(layout);
}

AVA_API int ava_ui_get_layout(AvaComponent* comp) {
    if (!comp) return 0;
    return comp->comp->GetLayout();
}

AVA_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root) {
    if (!tree || !root) return;
    tree->tree->SetRoot(root->comp);
}

AVA_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree) {
    if (!tree) return nullptr;
    auto root = tree->tree->GetRoot();
    if (!root) return nullptr;
    auto* wrapper = new AvaComponent(root->GetType());
    wrapper->comp = root;
    return wrapper;
}

AVA_API const char* ava_ui_get_component_type(AvaComponent* comp) {
    if (!comp) return nullptr;
    static thread_local std::string type_str;
    type_str = comp->comp->GetType();
    return type_str.c_str();
}

static void ComponentToJson(std::ostream& os, ava::ui::Component* comp, int indent) {
    if (!comp) return;
    std::string pad(indent * 2, ' ');
    os << pad << "{\n";
    os << pad << "  \"type\": \"" << comp->GetType() << "\"";
    if (!comp->GetId().empty()) {
        os << ",\n" << pad << "  \"id\": \"" << comp->GetId() << "\"";
    }
    os << ",\n" << pad << "  \"layout\": " << comp->GetLayout();
    const auto& props = comp->GetAllProperties();
    if (!props.empty()) {
        os << ",\n" << pad << "  \"properties\": {";
        bool first = true;
        for (const auto& [k, v] : props) {
            if (!first) os << ", ";
            os << "\"" << k << "\": ";
            if (v.type == ava::ValueType::String) {
                os << "\"" << "\"";
            } else if (v.type == ava::ValueType::Number) {
                os << v.n;
            } else if (v.type == ava::ValueType::Bool) {
                os << (v.b ? "true" : "false");
            } else {
                os << "null";
            }
            first = false;
        }
        os << "}";
    }
    const auto& children = comp->GetChildren();
    if (!children.empty()) {
        os << ",\n" << pad << "  \"children\": [\n";
        for (size_t i = 0; i < children.size(); ++i) {
            ComponentToJson(os, children[i].get(), indent + 2);
            if (i < children.size() - 1) os << ",";
            os << "\n";
        }
        os << pad << "  ]";
    }
    os << "\n" << pad << "}";
}

AVA_API const char* ava_ui_tree_to_json(AvaComponentTree* tree) {
    if (!tree) return "";
    static thread_local std::ostringstream oss;
    oss.str(""); oss.clear();
    auto root = tree->tree->GetRoot();
    if (root) {
        ComponentToJson(oss, root.get(), 0);
    }
    static thread_local std::string result;
    result = oss.str();
    return result.c_str();
}

AVA_API void ava_ui_json_free(char* json) {
    (void)json;
}

} // extern "C"
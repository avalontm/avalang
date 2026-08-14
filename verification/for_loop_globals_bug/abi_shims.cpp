// Shim minimo: subconjunto de la ABI de api/src/c_api.cpp (solo lo que
// builtin_lists/dicts/strings.cpp necesitan) + un stub de CompileSource
// (normalmente en frontend_antlr.cpp, requiere ANTLR4 que no esta
// disponible en este sandbox de verificacion). No forma parte del
// proyecto: es infraestructura de prueba ad-hoc para este harness.
#include "vm/value.h"
#include "avalang.h"
#include <cstring>
#include <string>

using namespace ava;

extern "C" {

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
    if (out_entries) *out_entries = d->c_entries_cache.data();
    return d->c_entries_cache.size();
}

AVA_API int ava_dict_contains(AvaVM*, ava_value_t dict, const char* key, size_t key_len) {
    Value v = FromC(dict);
    auto* d = static_cast<DictObj*>(v.obj);
    std::string k(key, key_len);
    return d->index.find(k) != d->index.end() ? 1 : 0;
}

// Stub: nada en este harness llama VM::RunFile / VM::DoImport (usamos
// Compiler::Compile directo sobre AST armado a mano), pero vm_file.o /
// vm_import.o referencian este simbolo y hace falta satisfacer el
// linker. Un stub que tira excepcion si alguna vez se llama es mas
// seguro que un stub silencioso.
std::shared_ptr<ava::Proto> CompileSourceStubImpl(const std::string&, const std::string&);
}

namespace ava {
std::shared_ptr<Proto> CompileSource(const std::string&, const std::string&) {
    throw std::runtime_error("CompileSource no disponible en este harness (sin frontend ANTLR); "
                              "este harness solo compila AST armado a mano via Compiler::Compile");
}
}

#ifndef AVA_VM_VALUE_H
#define AVA_VM_VALUE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>

#include "../../api/include/avalang.h"

namespace ava {

enum class ValueType : uint8_t {
    Nil = 0,
    Bool,
    Number,
    String,
    List,
    Dict,
    Function,
    Instance,
    Class,
    Coroutine,
    Native,
    Bound,
    Exception,
    Module
};

struct Object {
    std::atomic<int64_t> ref_count{1};
    virtual ~Object() = default;
};

struct StringObj : Object {
    std::string data;
    explicit StringObj(std::string s) : data(std::move(s)) {}
};

struct Value; // fwd decl

struct ListObj : Object {
    std::vector<Value> items;
};

struct DictObj : Object {
    // Insertion-ordered map: vector of keys + hash index, mirrors Lua/Python
    // dict semantics well enough for a v1. Keys are always strings for now;
    // numeric keys can be added later without touching the C API surface.
    std::vector<std::pair<std::string, Value>> entries;
    std::unordered_map<std::string, size_t> index;

    // ava_dict_entries() (c_api.cpp) hands callers a `ava_dict_pair_t*` --
    // a C-ABI-stable {const char* key; size_t key_len; ava_value_t value;}
    // struct that does NOT share layout with `entries` above (std::string
    // + Value have a completely different byte layout, e.g. std::string's
    // SSO buffer read as a pointer, or trailing Value bytes read as
    // key_len). That mismatch was previously papered over by reinterpret-
    // casting entries.data() directly, which handed out garbage
    // (frequently a huge bogus key_len that blew up as std::bad_alloc the
    // moment a caller did `std::string(key, key_len)`). c_entries_cache
    // is the real, correctly-laid-out buffer ava_dict_entries() rebuilds
    // and returns a pointer into -- it lives as long as the DictObj so
    // the pointer stays valid for the caller's read-only, non-reentrant
    // use immediately after the call.
    std::vector<ava_dict_pair_t> c_entries_cache;
};

struct Proto;   // compiler/proto.h
struct Closure;
struct ClassObj;
struct BoundMethod;

// Native (host) function pointer, matches AvaNativeFn from ava.h.
struct NativeObj : Object {
    AvaNativeFn fn;
    void* user_data;
    bool is_primitive_method = false;
    ava_value_t primitive_this{};
};

struct ClassObj : Object {
    std::string name;
    // Compartido por TODAS las instancias -- vive solo acá, nunca se
    // copia dentro de un InstanceObj. Incluye los atributos declarados
    // `static`, más las claves internas de bookkeeping ("__base__").
    // Ver DISENO_visibilidad_clases_avalang.md, Fase C/D.
    std::unordered_map<std::string, Value> attrs;
    // Valores por defecto de instancia: SÍ se copian a cada InstanceObj
    // nuevo (uno por instancia, independientes entre sí). Antes de la
    // Fase C, esto vivía mezclado dentro de `attrs`, lo que hacía que
    // `Clase.x = valor` no compartiera estado real con instancias ya
    // creadas -- ver value.h/vm.cpp para el detalle del bug corregido.
    std::unordered_map<std::string, Value> instance_defaults;
    std::unordered_map<std::string, std::shared_ptr<Proto>> methods;
    // Nombres de atributos y métodos marcados `private`. Es solo
    // metadata: en esta fase el VM no la usa para negar acceso (ver
    // §6 del documento de diseño -- el enforcement de `private` queda
    // para Ava Studio en la Fase E, no para el VM).
    std::unordered_set<std::string> private_members;
    std::vector<std::string> param_names;
    ClassObj* base_class = nullptr;
};

struct InstanceObj : Object {
    ClassObj* cls;
    std::unordered_map<std::string, Value> attrs;
};

// Namespace creado por un bloque `extern "lib" as Alias ... end`
// (ver EXTERN_FFI_DESIGN.md). `attrs` mapea nombre de función declarada ->
// Value de tipo Native cuyo NativeObj::user_data apunta a un
// ExternFuncMeta (ver vm/vm_extern.h). No es instanciable (a diferencia de
// Class) ni tiene métodos con `this`: es solo un contenedor de funciones.
struct ModuleObj : Object {
    std::string name;   // alias, p.ej. "Kernel"
    std::string library; // nombre lógico de librería, p.ej. "kernel32"
    std::unordered_map<std::string, Value> attrs;
};

// A single dynamically-typed value. Ref-counted objects (String/List/Dict/
// Function/Instance/Class/Coroutine/Native) are stored as an Object* behind
// a manual ref count; Value itself is cheap to copy (16 bytes: tag + union).
struct Value {
    ValueType type = ValueType::Nil;
    union {
        bool     b;
        double   n;
        Object*  obj;
    };

    Value() : type(ValueType::Nil), n(0) {}
    static Value Nil()                 { Value v; v.type = ValueType::Nil; return v; }
    static Value Bool(bool x)          { Value v; v.type = ValueType::Bool; v.b = x; return v; }
    static Value Number(double x)      { Value v; v.type = ValueType::Number; v.n = x; return v; }
    static Value String(const std::string& s) { Value v; v.type = ValueType::String; v.obj = new StringObj(s); return v; }
    static Value Coroutine(void* co) { Value v; v.type = ValueType::Coroutine; v.obj = static_cast<Object*>(co); return v; }

    bool IsTruthy() const {
        switch (type) {
            case ValueType::Nil:   return false;
            case ValueType::Bool:  return b;
            case ValueType::Number: return n != 0;
            default:               return true;
        }
    }

    bool IsRefCounted() const {
        switch (type) {
            case ValueType::String:
            case ValueType::List:
            case ValueType::Dict:
            case ValueType::Function:
            case ValueType::Instance:
            case ValueType::Class:
            case ValueType::Native:
            case ValueType::Bound:
            case ValueType::Exception:
            case ValueType::Module:
                return true;
            default:
                return false;
        }
    }
};

struct BoundMethod : Object {
    std::shared_ptr<Proto> proto;
    Value instance;
};

struct ExceptionObj : Object {
    std::string type;
    std::string message;
};

void Retain(const Value& v);
void Release(const Value& v);

// Conversions to/from the public C ABI struct (ava_value_t). These are the
// only functions that should touch AvaRef.id <-> Object* directly.
AVA_API ava_value_t ToC(const Value& v);
AVA_API Value FromC(const ava_value_t& v);

} // namespace ava

#endif // AVA_VM_VALUE_H

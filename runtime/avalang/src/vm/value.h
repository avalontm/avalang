#ifndef AVA_VM_VALUE_H
#define AVA_VM_VALUE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

#include "../../api/include/avalang.h"

namespace ava {

enum class ValueType : avastd::uint8_t {
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
    Module,
    Task
};

// Sub-fase 4 de Fase 5 (GC), "Registrar objetos" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §5. Todo Object vivo se
// registra en una lista intrusiva global al construirse y se desregistra
// al destruirse (gc_prev/gc_next, definido en value.cpp). Es intrusivo a
// proposito -- ningun sitio de allocacion (`new StringObj(...)`, etc., hay
// ~35 repartidos en VM/compiler/builtins/C API) necesita tocarse: el
// registro pasa por el constructor/destructor de Object mismo, igual que
// el ownership de sub-fase 2 paso por el de Value. Esta lista es la base
// que "Crear roots" e "Implementar tracing" (proximas sub-fases) van a
// recorrer para el cycle collector -- todavia no la usa nadie mas que
// GcLiveObjectCount()/GcForEachObject(), que existen para poder verificar
// esta sub-fase de forma aislada.
// Sub-fase 7 de Fase 5 (GC), "Manejar ciclos". Separado a proposito de
// ValueType (que alimenta la C ABI publica via ToC()/AvaValueType y no se
// puede tocar sin riesgo de romper el contrato binario). El sweep de
// ciclos necesita saber el tipo dinamico real de cada Object visitado via
// GcForEachObject (que solo da Object*, sin tag), y GcObjectKind es ese
// tag, guardado en el propio Object en vez de en el Value que lo apunta.
enum class GcObjectKind : avastd::uint8_t {
    String = 0,
    List,
    Dict,
    Function,
    Instance,
    Class,
    Native,
    Bound,
    Exception,
    Module,
    Upvalue
};

struct Object {
    avastd::atomic<avastd::int64_t> ref_count{1};
    Object* gc_prev = nullptr;
    Object* gc_next = nullptr;
    GcObjectKind gc_kind;

    AVA_API explicit Object(GcObjectKind kind);
    AVA_API virtual ~Object();
};

// Cantidad de Object vivos ahora mismo (todo lo registrado y no
// destruido). O(1): se mantiene como contador aparte, no recorre la lista.
AVA_API avastd::int64_t GcLiveObjectCount();

// Visita cada Object vivo ahora mismo (snapshot bajo el mismo lock del
// registro, no reentrante: `visit` no debe crear ni destruir Objects).
// `ctx` se pasa tal cual a cada llamada -- pensado para que "Crear roots"/
// "Implementar tracing" lo usen para marcar, sin que este archivo necesite
// saber nada de esos algoritmos.
AVA_API void GcForEachObject(void (*visit)(Object*, void*), void* ctx);

struct StringObj : Object {
    avastd::string data;
    explicit StringObj(avastd::string s) : Object(GcObjectKind::String), data(avastd::move(s)) {}
};

struct Value; // fwd decl

struct ListObj : Object {
    ListObj() : Object(GcObjectKind::List) {}
    avastd::vector<Value> items;
};

struct DictObj : Object {
    DictObj() : Object(GcObjectKind::Dict) {}
    // Insertion-ordered map: vector of keys + hash index, mirrors Lua/Python
    // dict semantics well enough for a v1. Keys are always strings for now;
    // numeric keys can be added later without touching the C API surface.
    avastd::vector<avastd::pair<avastd::string, Value>> entries;
    avastd::unordered_map<avastd::string, size_t> index;

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
    avastd::vector<ava_dict_pair_t> c_entries_cache;
};

struct Proto;   // compiler/proto.h
struct Closure;
struct ClassObj;
struct BoundMethod;

// Native (host) function pointer, matches AvaNativeFn from ava.h.
struct NativeObj : Object {
    NativeObj() : Object(GcObjectKind::Native) {}
    // Sub-fase 10 de Fase 5 (GC), "Integrar native resources" -- ver
    // docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §11. `primitive_this`
    // (cuando `is_primitive_method`) guarda una referencia retenida real
    // (ver vm_classes.cpp::OpGetAttr) -- hace falta destructor propio para
    // soltarla, ya que es un `ava_value_t` (C ABI), no un `Value` con RAII.
    // Definido en value.cpp (necesita Release()/FromC(), declarados mas
    // abajo en este mismo header).
    ~NativeObj() override;
    AvaNativeFn fn;
    void* user_data;
    bool is_primitive_method = false;
    ava_value_t primitive_this{};
};

struct ClassObj : Object {
    ClassObj() : Object(GcObjectKind::Class) {}
    avastd::string name;
    // Compartido por TODAS las instancias -- vive solo acá, nunca se
    // copia dentro de un InstanceObj. Incluye los atributos declarados
    // `static`, más las claves internas de bookkeeping ("__base__").
    // Ver DISENO_visibilidad_clases_avalang.md, Fase C/D.
    avastd::unordered_map<avastd::string, Value> attrs;
    // Valores por defecto de instancia: SÍ se copian a cada InstanceObj
    // nuevo (uno por instancia, independientes entre sí). Antes de la
    // Fase C, esto vivía mezclado dentro de `attrs`, lo que hacía que
    // `Clase.x = valor` no compartiera estado real con instancias ya
    // creadas -- ver value.h/vm.cpp para el detalle del bug corregido.
    avastd::unordered_map<avastd::string, Value> instance_defaults;
    avastd::unordered_map<avastd::string, avastd::shared_ptr<Proto>> methods;
    // Nombres de atributos y métodos marcados `private`. Es solo
    // metadata: en esta fase el VM no la usa para negar acceso (ver
    // §6 del documento de diseño -- el enforcement de `private` queda
    // para Ava Studio en la Fase E, no para el VM).
    avastd::unordered_set<avastd::string> private_members;
    avastd::vector<avastd::string> param_names;
    ClassObj* base_class = nullptr;
};

struct InstanceObj : Object {
    InstanceObj() : Object(GcObjectKind::Instance) {}
    ClassObj* cls;
    avastd::unordered_map<avastd::string, Value> attrs;
};

// Namespace creado por un bloque `extern "lib" as Alias ... end`
// (ver EXTERN_FFI_DESIGN.md). `attrs` mapea nombre de función declarada ->
// Value de tipo Native cuyo NativeObj::user_data apunta a un
// ExternFuncMeta (ver vm/vm_extern.h). No es instanciable (a diferencia de
// Class) ni tiene métodos con `this`: es solo un contenedor de funciones.
struct ModuleObj : Object {
    ModuleObj() : Object(GcObjectKind::Module) {}
    avastd::string name;   // alias, p.ej. "Kernel"
    avastd::string library; // nombre lógico de librería, p.ej. "kernel32"
    avastd::unordered_map<avastd::string, Value> attrs;
};

// Forward declarations: Value's own copy/move/destructor (below) need
// these before the struct is closed. Full declarations (same signature)
// also appear after the struct for readability/back-compat with existing
// call sites -- both refer to the same functions defined in value.cpp.
AVA_API void Retain(const Value& v);
AVA_API void Release(const Value& v);

// A single dynamically-typed value. Ref-counted objects (String/List/Dict/
// Function/Instance/Class/Coroutine/Native) are stored as an Object* behind
// a manual ref count; Value itself is cheap to copy (16 bytes: tag + union).
//
// Sub-fase 2 de Fase 5 (GC), ver docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md:
// Value ahora es RAII -- copiar/mover/destruir un Value retiene/libera su
// Object* automaticamente (cuando IsRefCounted() es true). Esto hace que
// TODO contenedor que ya guarda Value por copia (ListObj::items,
// DictObj::entries, ClassObj::attrs/instance_defaults, InstanceObj::attrs,
// ModuleObj::attrs, CallFrame::registers, etc.) empiece a retener/liberar
// correctamente sin tocar esos sitios uno por uno -- avastd::vector y
// avastd::unordered_map ya construyen/destruyen elementos de verdad
// (placement-new + ~T() explicito, ver ava_vector.h), y en build hosted
// son alias de std::vector/std::unordered_map (mismo comportamiento).
//
// Sub-fase 3 (cerrada), ver docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md
// §3.1: los ~37 call sites que hacian Retain()/Release() manual
// (vm_core.cpp globals_, vm_arith.cpp concat de listas,
// vm.cpp/vm_call_op.cpp/vm_classes.cpp/vm_import.cpp/vm_task.cpp
// frames/upvalues/coroutines) ya fueron auditados y limpiados -- no
// queda ningun Retain()/Release() manual fuera de este archivo y de
// value.cpp en todo src/. Varios de esos sitios no eran leaks inocuos
// como se esperaba: eran doble-release real (Release() manual justo
// antes de una asignacion de Value que ya libera el valor viejo por su
// cuenta), corregido en cada caso. Los ~25 sitios de mutacion de
// contenedores que NO retenian nada antes (ver el documento de diseno)
// tambien quedan corregidos: ese era el bug original (use-after-free),
// y es el que este cambio soluciona.
struct Value {
    ValueType type = ValueType::Nil;
    union {
        bool     b;
        double   n;
        Object*  obj;
    };

    Value() : type(ValueType::Nil), n(0) {}

    // Copia: retiene la referencia que esta nueva copia posee.
    Value(const Value& other) : type(other.type) {
        CopyUnionFrom(other);
        Retain(*this);
    }

    // Movimiento: transfiere la referencia sin retener/liberar -- `other`
    // queda en Nil (su destructor luego es un no-op).
    Value(Value&& other) noexcept : type(other.type) {
        CopyUnionFrom(other);
        other.type = ValueType::Nil;
        other.n = 0;
    }

    Value& operator=(const Value& other) {
        if (this == &other) return *this;
        // Retener la nueva referencia ANTES de liberar la vieja: si ambas
        // apuntan (indirectamente) al mismo objeto por otro camino, evita
        // liberarlo de mas antes de terminar de copiar.
        Retain(other);
        Release(*this);
        type = other.type;
        CopyUnionFrom(other);
        return *this;
    }

    Value& operator=(Value&& other) noexcept {
        if (this == &other) return *this;
        Release(*this);
        type = other.type;
        CopyUnionFrom(other);
        other.type = ValueType::Nil;
        other.n = 0;
        return *this;
    }

    ~Value() { Release(*this); }

    static Value Nil()                 { Value v; v.type = ValueType::Nil; return v; }
    static Value Bool(bool x)          { Value v; v.type = ValueType::Bool; v.b = x; return v; }
    static Value Number(double x)      { Value v; v.type = ValueType::Number; v.n = x; return v; }
    static Value String(const avastd::string& s) { Value v; v.type = ValueType::String; v.obj = new StringObj(s); return v; }
    static Value Coroutine(void* co) { Value v; v.type = ValueType::Coroutine; v.obj = static_cast<Object*>(co); return v; }
    static Value Task(void* t) { Value v; v.type = ValueType::Task; v.obj = static_cast<Object*>(t); return v; }

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

private:
    // Copia el miembro activo de la union segun `type` (ya asignado por
    // el caller). Para Nil copiar `obj` es inofensivo -- no se dereferencia
    // (IsRefCounted() da false), solo iguala los bytes crudos.
    void CopyUnionFrom(const Value& other) {
        switch (type) {
            case ValueType::Bool:   b = other.b; break;
            case ValueType::Number: n = other.n; break;
            default:                obj = other.obj; break;
        }
    }
};

struct BoundMethod : Object {
    BoundMethod() : Object(GcObjectKind::Bound) {}
    avastd::shared_ptr<Proto> proto;
    Value instance;
};

struct ExceptionObj : Object {
    ExceptionObj() : Object(GcObjectKind::Exception) {}
    avastd::string type;
    avastd::string message;
};

// Retain()/Release() declared above (before Value's definition, needed
// there for its copy/move ctors). Defined in value.cpp.

// Conversions to/from the public C ABI struct (ava_value_t). These are the
// only functions that should touch AvaRef.id <-> Object* directly.
AVA_API ava_value_t ToC(const Value& v);
AVA_API Value FromC(const ava_value_t& v);

} // namespace ava

#endif // AVA_VM_VALUE_H

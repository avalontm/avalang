#include "value.h"

namespace ava {

namespace {
// Estado del registro intrusivo (sub-fase 4, "Registrar objetos"). Vive
// solo acá como estáticos de TU -- Object::Object()/~Object() son los
// únicos que lo tocan, así que no hay problema de orden de inicialización
// entre TUs (los objetos se construyen en tiempo de ejecución, no durante
// static init de otro archivo).
avastd::mutex g_gc_registry_mutex;
Object* g_gc_registry_head = nullptr;
avastd::atomic<avastd::int64_t> g_gc_live_count{0};
}  // namespace

Object::Object(GcObjectKind kind) : gc_kind(kind) {
    avastd::lock_guard<avastd::mutex> lock(g_gc_registry_mutex);
    gc_prev = nullptr;
    gc_next = g_gc_registry_head;
    if (g_gc_registry_head) g_gc_registry_head->gc_prev = this;
    g_gc_registry_head = this;
    g_gc_live_count.fetch_add(1, avastd::memory_order_relaxed);
}

Object::~Object() {
    avastd::lock_guard<avastd::mutex> lock(g_gc_registry_mutex);
    if (gc_prev) gc_prev->gc_next = gc_next;
    else g_gc_registry_head = gc_next;
    if (gc_next) gc_next->gc_prev = gc_prev;
    gc_prev = nullptr;
    gc_next = nullptr;
    g_gc_live_count.fetch_sub(1, avastd::memory_order_relaxed);
}

avastd::int64_t GcLiveObjectCount() {
    return g_gc_live_count.load(avastd::memory_order_relaxed);
}

void GcForEachObject(void (*visit)(Object*, void*), void* ctx) {
    avastd::lock_guard<avastd::mutex> lock(g_gc_registry_mutex);
    for (Object* o = g_gc_registry_head; o != nullptr; o = o->gc_next) {
        visit(o, ctx);
    }
}

void Retain(const Value& v) {
    if (v.IsRefCounted() && v.obj) {
        v.obj->ref_count.fetch_add(1, avastd::memory_order_relaxed);
    }
}

void Release(const Value& v) {
    if (v.IsRefCounted() && v.obj) {
        if (v.obj->ref_count.fetch_sub(1, avastd::memory_order_acq_rel) == 1) {
            delete v.obj;
        }
    }
}

// AvaRef.id is the Object* reinterpreted as an integer handle. This keeps
// ava_value_t a flat POD struct with no pointer-typed field that bindings
// would need special-case marshalling for (works uniformly as uint64 across
// C#/Python/Java FFI layers).
ava_value_t ToC(const Value& v) {
    ava_value_t out{};
    out.type = static_cast<AvaValueType>(v.type);
    switch (v.type) {
        case ValueType::Nil:    break;
        case ValueType::Bool:   out.as.b = v.b ? 1 : 0; break;
        case ValueType::Number: out.as.n = v.n; break;
        default:
            out.as.ref.id = reinterpret_cast<uint64_t>(v.obj);
            break;
    }
    return out;
}

Value FromC(const ava_value_t& v) {
    Value out;
    out.type = static_cast<ValueType>(v.type);
    switch (out.type) {
        case ValueType::Nil:    break;
        case ValueType::Bool:   out.b = v.as.b != 0; break;
        case ValueType::Number: out.n = v.as.n; break;
        default:
            out.obj = reinterpret_cast<Object*>(v.as.ref.id);
            break;
    }
    return out;
}

// Sub-fase 10 de Fase 5 (GC), "Integrar native resources" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §11. Contraparte del
// Retain() que OpGetAttr (vm_classes.cpp) hace sobre el objeto primitivo
// al armar el NativeObj de un metodo bindeado (`"abc".upper`, etc.).
// GcClearRefs (gc_sweep.cpp) ya se encarga del caso de basura de ciclo
// -- ahi deja `is_primitive_method = false` antes de que este destructor
// corra, asi que el `if` de abajo no duplica el Release() en ese camino.
NativeObj::~NativeObj() {
    if (is_primitive_method) Release(FromC(primitive_this));
}

} // namespace ava

#include "gc_trace.h"
#include "closure.h"

namespace ava {

namespace {

void MarkValue(const Value& v, avastd::unordered_set<Object*>& marked);

// `type` viene de afuera (del Value que apuntaba a `obj`, o pasado a
// mano para aristas estructurales que no son Value -- ver los dos casos
// de abajo que llaman MarkObject directo) porque Object en si no sabe
// que tipo concreto es sin RTTI/virtual dispatch propio: el tag vive en
// Value, no en Object. Mismo truco que Value::IsRefCounted() -- el tipo
// dinamico real es el `type` con el que se llego hasta acá.
void MarkObject(Object* obj, ValueType type, avastd::unordered_set<Object*>& marked) {
    if (!obj) return;
    if (marked.find(obj) != marked.end()) return; // ya visitado -- corta ciclos
    marked.insert(obj);

    switch (type) {
        case ValueType::List: {
            auto* list = static_cast<ListObj*>(obj);
            for (auto& item : list->items) MarkValue(item, marked);
            break;
        }
        case ValueType::Dict: {
            auto* dict = static_cast<DictObj*>(obj);
            for (auto& kv : dict->entries) MarkValue(kv.second, marked);
            break;
        }
        case ValueType::Function: {
            auto* closure = static_cast<Closure*>(obj);
            for (auto& up : closure->upvalues) {
                if (!up) continue;
                // Upvalue no es un ValueType (closure.h lo define como
                // Object aparte, guardado via shared_ptr, no via Value
                // con tag) -- se marca directo, no por MarkObject con un
                // `type` inventado.
                if (marked.find(up.get()) != marked.end()) continue;
                marked.insert(up.get());
                MarkValue(up->value, marked);
                // up->location apunta a un registro de frame mientras el
                // upvalue sigue "abierto" -- no es una arista de
                // ownership (el frame es dueño de ese Value, no el
                // Upvalue), y ese Value ya es raíz por su cuenta
                // (registro de frame, sub-fase 5) o deja de existir
                // cuando el frame cierra, momento en el que el upvalue
                // ya debería tener su copia propia en `value`. Seguir
                // ese puntero acá sería recorrer memoria que no le
                // pertenece a este objeto.
            }
            break;
        }
        case ValueType::Instance: {
            auto* inst = static_cast<InstanceObj*>(obj);
            for (auto& kv : inst->attrs) MarkValue(kv.second, marked);
            // inst->cls es un ClassObj* crudo, NO retenido (ver
            // vm_classes.cpp::OpNewInstance: `inst->cls = cls;` sin
            // Retain) -- un gap de ownership real que ya existía antes
            // de este cambio, no algo que esta sub-fase introduce. Pero
            // para el grafo de "qué sigue vivo" sí es una arista real:
            // si el ClassObj se liberase mientras esta instancia lo
            // sigue apuntando, quedaría un puntero colgante. Se marca
            // igual para que un futuro sweep nunca lo barra mientras
            // haya una instancia viva -- eso no arregla el gap de
            // refcounting en sí (ese arreglo, con su propio Retain() en
            // OpNewInstance y Release() en ~InstanceObj, es tarea
            // aparte, documentada, no de esta sub-fase de tracing).
            if (inst->cls) MarkObject(inst->cls, ValueType::Class, marked);
            break;
        }
        case ValueType::Class: {
            auto* cls = static_cast<ClassObj*>(obj);
            for (auto& kv : cls->attrs) MarkValue(kv.second, marked);
            for (auto& kv : cls->instance_defaults) MarkValue(kv.second, marked);
            // Mismo caso que InstanceObj::cls arriba: base_class es un
            // ClassObj* crudo, no retenido, pero es una arista real del
            // grafo de vida de una clase derivada.
            if (cls->base_class) MarkObject(cls->base_class, ValueType::Class, marked);
            break;
        }
        case ValueType::Module: {
            auto* mod = static_cast<ModuleObj*>(obj);
            for (auto& kv : mod->attrs) MarkValue(kv.second, marked);
            break;
        }
        case ValueType::Bound: {
            auto* bound = static_cast<BoundMethod*>(obj);
            MarkValue(bound->instance, marked);
            break;
        }
        case ValueType::String:
        case ValueType::Exception:
            // Hojas: StringObj/ExceptionObj no guardan ningun Value hijo.
            break;
        case ValueType::Native:
            // NativeObj::primitive_this es un ava_value_t (C ABI), no un
            // Value -- convertirlo con FromC() (value.cpp) construiría un
            // Value temporal cuyo destructor llamaría Release() sobre un
            // Object que este tracing nunca retuvo, un side effect real
            // (baja un refcount de más) que no tiene nada que ver con
            // "solo marcar". Se deja sin recorrer a propósito en vez de
            // arriesgar eso; NativeObj::user_data es un puntero opaco de
            // host, tampoco recorrible. Ver "Integrar native resources"
            // (checklist de Fase 5) para la sub-fase que sí debe resolver
            // esto con su propio mecanismo, no reusando FromC.
            break;
        default:
            // Nil/Bool/Number/Coroutine/Task: ya filtrados por
            // MarkValue() via IsRefCounted(), o (Coroutine/Task) no son
            // Object -- no deberían llegar acá nunca.
            break;
    }
}

void MarkValue(const Value& v, avastd::unordered_set<Object*>& marked) {
    if (!v.IsRefCounted() || !v.obj) return;
    MarkObject(v.obj, v.type, marked);
}

}  // namespace

avastd::size_t GcTraceMark(const avastd::vector<Value*>& roots,
                            avastd::unordered_set<Object*>& out_marked) {
    for (Value* root : roots) {
        if (root) MarkValue(*root, out_marked);
    }
    return out_marked.size();
}

} // namespace ava

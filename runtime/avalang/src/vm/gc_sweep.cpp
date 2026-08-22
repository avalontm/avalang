#include "gc_sweep.h"
#include "gc_trace.h"
#include "closure.h"
#include "vm.h"

namespace ava {

namespace {

void GcClearRefs(Object* obj) {
    switch (obj->gc_kind) {
        case GcObjectKind::List:
            static_cast<ListObj*>(obj)->items.clear();
            break;
        case GcObjectKind::Dict: {
            auto* dict = static_cast<DictObj*>(obj);
            dict->entries.clear();
            dict->index.clear();
            dict->c_entries_cache.clear();
            break;
        }
        case GcObjectKind::Function:
            static_cast<Closure*>(obj)->upvalues.clear();
            break;
        case GcObjectKind::Instance:
            static_cast<InstanceObj*>(obj)->attrs.clear();
            break;
        case GcObjectKind::Class: {
            auto* cls = static_cast<ClassObj*>(obj);
            cls->attrs.clear();
            cls->instance_defaults.clear();
            break;
        }
        case GcObjectKind::Bound:
            static_cast<BoundMethod*>(obj)->instance = Value();
            break;
        case GcObjectKind::Module:
            static_cast<ModuleObj*>(obj)->attrs.clear();
            break;
        case GcObjectKind::Upvalue:
            static_cast<Upvalue*>(obj)->value = Value();
            break;
        case GcObjectKind::Native: {
            // Sub-fase 10 de Fase 5 (GC), "Integrar native resources" --
            // ver docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §11. Si
            // este NativeObj retiene un `this` primitivo (OpGetAttr,
            // vm_classes.cpp), soltarlo ACA -- protegido por el bias de
            // ref_count +1 aplicado a toda la basura antes de este paso,
            // igual que el resto de los casos de este switch -- y no en
            // ~NativeObj() (value.cpp), que corre despues, en el `delete`
            // sin bias del paso 3: soltarlo ahi tambien seria un
            // double-release si el objetivo es otro miembro del mismo
            // ciclo de basura. `is_primitive_method = false` deja el
            // destructor como no-op para este objeto.
            auto* native = static_cast<NativeObj*>(obj);
            if (native->is_primitive_method) {
                Release(FromC(native->primitive_this));
                native->is_primitive_method = false;
            }
            break;
        }
        case GcObjectKind::String:
        case GcObjectKind::Exception:
            break;
    }
}

void CollectAllObjects(Object* obj, void* ctx) {
    static_cast<avastd::vector<Object*>*>(ctx)->push_back(obj);
}

}  // namespace

GcSweepStats GcCollectCycles(VM& vm) {
    GcSweepStats stats;

    avastd::vector<Value*> roots;
    vm.CollectGcRoots(roots);

    avastd::unordered_set<Object*> marked;
    GcTraceMark(roots, marked);
    stats.marked = static_cast<avastd::int64_t>(marked.size());

    avastd::vector<Object*> snapshot;
    GcForEachObject(CollectAllObjects, &snapshot);
    stats.objects_before = static_cast<avastd::int64_t>(snapshot.size());

    avastd::vector<Object*> garbage;
    for (Object* obj : snapshot) {
        if (obj->gc_kind == GcObjectKind::Upvalue) continue;
        if (marked.find(obj) != marked.end()) continue;
        garbage.push_back(obj);
    }

    for (Object* obj : garbage) {
        obj->ref_count.fetch_add(1, avastd::memory_order_relaxed);
    }
    for (Object* obj : garbage) {
        GcClearRefs(obj);
    }
    for (Object* obj : garbage) {
        delete obj;
    }

    stats.collected = static_cast<avastd::int64_t>(garbage.size());
    return stats;
}

} // namespace ava

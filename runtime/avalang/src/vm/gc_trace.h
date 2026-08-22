#ifndef AVA_VM_GC_TRACE_H
#define AVA_VM_GC_TRACE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "value.h"

namespace ava {

// Sub-fase 6 de Fase 5 (GC), "Implementar tracing" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §7.
//
// Marca como alcanzable todo Object llegable desde `roots` (los Value*
// que arma VM::CollectGcRoots / CollectCoroutineRoots / CollectTaskRoots,
// sub-fase 5), recorriendo los Value que cada contenedor guarda
// (ListObj::items, DictObj::entries, ClassObj::attrs/instance_defaults,
// InstanceObj::attrs, ModuleObj::attrs, BoundMethod::instance, upvalues
// de Closure) MAS un par de punteros estructurales que no pasan por
// Value pero siguen siendo parte real del grafo de vida
// (InstanceObj::cls, ClassObj::base_class) -- ver el comentario junto a
// esos casos en gc_trace.cpp para el porque.
//
// `out_marked` es el set de visitados (evita recursion infinita en
// ciclos, que es exactamente el caso que esto existe para poder
// detectar) y, a la vez, el resultado: todo lo que termina adentro está
// vivo. Se puede pasar vacío. Devuelve `out_marked.size()` por
// comodidad.
//
// Esta sub-fase es SOLO el marcado -- de solo lectura, no libera nada.
// El barrido (recorrer GcForEachObject y borrar lo no marcado) es la
// sub-fase siguiente, "Manejar ciclos", que además necesita decidir
// *cuándo* correr esto (no en cada Release(), sería carísimo) -- acá
// todavía no se resuelve eso a propósito.
avastd::size_t GcTraceMark(const avastd::vector<Value*>& roots,
                            avastd::unordered_set<Object*>& out_marked);

} // namespace ava

#endif // AVA_VM_GC_TRACE_H

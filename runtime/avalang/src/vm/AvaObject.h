#ifndef AVA_VM_AVA_OBJECT_H
#define AVA_VM_AVA_OBJECT_H

#include "value.h"

// Fase 2 del plan, seccion 4.1: "AvaObject debe representar objetos
// administrados por el runtime, y permitir evolucionar hacia clases,
// herencia, instancias, propiedades, metodos, metadata y GC."
//
// Decision (cierra el punto abierto en RUNTIME_CORE_AUDIT.md Sec.5): ese
// tipo ya existe -- es `ava::Object` (value.h): ref-count atomico,
// registro intrusivo en la lista global de vivos (gc_prev/gc_next,
// Fase 5 sub-fase 4), `GcObjectKind` como tag de tipo dinamico para el
// tracer/sweep de ciclos (Fase 5 sub-fase 6/7), y destructor virtual para
// que `delete` sobre un `Object*` despache al tipo real. La evolucion que
// pide esta seccion ya existe tambien, no es futura: `ClassObj`
// (clases/herencia via `parent`/metodos), `Instance` (instancias +
// propiedades via `fields`), `BoundMethod` (metodos bindeados) y
// `ModuleObj`/`ExceptionObj` (metadata) heredan todos de `Object` en el
// mismo archivo. Construir un `AvaObject` nuevo duplicaria el ownership
// GC que Fase 5 ya cerro -- mismo razonamiento que AvaValue.h.
//
// `AvaObject` es el nombre estable que pide el plan para este tipo ya
// existente.
namespace ava {

using AvaObject = Object;

} // namespace ava

#endif // AVA_VM_AVA_OBJECT_H

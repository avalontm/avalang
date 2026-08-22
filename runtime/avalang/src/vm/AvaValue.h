#ifndef AVA_VM_AVA_VALUE_H
#define AVA_VM_AVA_VALUE_H

#include "value.h"

// Fase 2 del plan (avalang_runtime_stl_barekernel_plan.md, seccion 4.1):
// "AvaValue representa los valores del lenguaje: nil, boolean, integer,
// number, string, array, map, object, function, native function,
// coroutine."
//
// Decision (cierra el punto abierto en RUNTIME_CORE_AUDIT.md Sec.5): ese
// tipo ya existe -- es `ava::Value` (value.h), con `ValueType` cubriendo
// Nil/Bool/Number/String/List/Dict/Function/Instance/Class/Coroutine/
// Native/Bound/Exception/Module/Task (superset de la lista minima de la
// seccion 4.1). No se construye un `AvaValue` nuevo por encima: `Value`
// ya es exactamente el tipo semantico que pide el plan -- 16 bytes
// (tag + union), copia barata, y Retain()/Release() automaticos via
// constructor de copia/movimiento/destructor (ver el diseno RAII descrito
// en docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md Sec.3). Duplicarlo
// en un tipo paralelo dividiria el ownership de objetos GC en dos
// implementaciones -- exactamente el bug que Fase 5 (GC) ya resolvio una
// vez (ver Sec.1 de ese mismo documento).
//
// `AvaValue` es entonces el nombre estable que pide el plan para este
// tipo ya existente -- para que codigo nuevo escrito contra la
// terminologia de la seccion 4 (`AvaValue`, `AvaObject`) compile igual
// que codigo escrito contra los nombres historicos (`Value`, `Object`).
namespace ava {

using AvaValue = Value;

} // namespace ava

#endif // AVA_VM_AVA_VALUE_H

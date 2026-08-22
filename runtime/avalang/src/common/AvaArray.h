#ifndef AVA_COMMON_AVA_ARRAY_H
#define AVA_COMMON_AVA_ARRAY_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

// Fase 2 del plan, seccion 4.1: "AvaArray debe sustituir progresivamente a
// std::vector, administrando elementos, tamanio, capacidad, insercion,
// eliminacion, indexacion e iteracion."
//
// Misma decision que AvaString.h: ya existe -- avastd::vector<T>
// (platform/barekernel/stdcompat/ava_vector.h). Hosted = std::vector real;
// BareKernel = reimplementacion freestanding propia (push_back/pop_back,
// operator[], size()/capacity(), begin()/end() para iteracion range-for)
// que aloca via ava_alloc/ava_realloc/ava_free. `ListObj::items`
// (src/vm/value.h) ya es un avastd::vector<Value> -- AvaArray<Value> y
// ListObj::items son, hoy, el mismo tipo concreto.
//
// Se deja como alias de plantilla (no de un solo tipo, como AvaString)
// porque a diferencia de un string el uso real en el runtime es siempre
// sobre distintos T (Value en ListObj, Instr en Proto, etc.) -- forzar un
// unico AvaArray no generico no reflejaria el uso actual del codigo.
namespace ava {

template <typename T>
using AvaArray = avastd::vector<T>;

} // namespace ava

#endif // AVA_COMMON_AVA_ARRAY_H

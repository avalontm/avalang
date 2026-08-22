#ifndef AVA_COMMON_AVA_MAP_H
#define AVA_COMMON_AVA_MAP_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

// Fase 2 del plan, seccion 4.1: "AvaMap debe sustituir progresivamente a
// std::unordered_map, administrando hash, buckets, insercion, busqueda,
// eliminacion e iteracion."
//
// Misma decision que AvaString.h/AvaArray.h: ya existe --
// avastd::unordered_map<K, V> (platform/barekernel/stdcompat/
// ava_unordered_map.h). Hosted = std::unordered_map real; BareKernel =
// tabla hash freestanding propia con especializaciones de avastd::Hash
// para avastd::string/int/int64_t (las unicas claves que usa el runtime
// hoy, ver comentario en ava_unordered_map.h) y buckets que alocan via
// ava_alloc/ava_free.
//
// Nota -- esto es el primitivo de mapa clave/valor generico, no
// `DictObj` (src/vm/value.h). `DictObj` es una estructura de mas alto
// nivel construida sobre dos AvaMap/AvaArray para dar orden de insercion
// (un AvaArray<pair<AvaString, Value>> con los pares en orden + un
// AvaMap<AvaString, size_t> como indice hacia ese array, igual que
// Python/Lua). AvaMap es el bloque de construccion; DictObj es el tipo de
// valor del lenguaje que lo usa -- no se colapsan en uno solo porque
// tienen contratos distintos (AvaMap no garantiza orden; el Dict de
// AvaLang si).
namespace ava {

template <typename K, typename V>
using AvaMap = avastd::unordered_map<K, V>;

} // namespace ava

#endif // AVA_COMMON_AVA_MAP_H

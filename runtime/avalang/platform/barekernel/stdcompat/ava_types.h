#ifndef AVA_STDCOMPAT_TYPES_H
#define AVA_STDCOMPAT_TYPES_H

#include "ava_platform_caps.h"

// Por que este header NO incluye <cstdint>/<stdint.h>/<climits>/etc:
//
// Se probo primero con <stdint.h> (razonando que es un "freestanding
// header" garantizado por el estandar). Eso rompio contra el toolchain
// real: su <stdint.h> es solo un WRAPPER que hace
// `#include_next <stdint.h>`, esperando que una libc (tipicamente newlib)
// provea la version completa mas abajo en el search path. Este toolchain
// fue construido con --without-headers (confirmado via
// `i686-elf-g++ -v`): no hay ninguna libc instalada, asi que ese
// include_next no encuentra nada y falla.
//
// La solucion robusta (no depende de si ESTE toolchain en particular
// decidio traer una libc o no) es no incluir ningun header en absoluto
// para los tipos de ancho fijo: usar directamente los macros que el
// COMPILADOR (GCC/Clang) predefine siempre, con o sin headers, con o sin
// -ffreestanding, con o sin libc -- son los mismos macros que <stdint.h>
// usa internamente para autogenerarse. Ver "Common Predefined Macros" en
// la documentacion de GCC (__INT8_TYPE__ .. __UINT64_TYPE__,
// __SIZE_TYPE__, __PTRDIFF_TYPE__, __INTPTR_TYPE__, __UINTPTR_TYPE__, y
// las variantes _MAX__ para los limites).
//
// CKM_CAP_LIBSTDCPP=1 (Windows/Linux/macOS): no se usa nada de este
// archivo, avastd:: usa directo <cstdint>/<climits> reales.

#if AVA_HAVE_STD_LIBRARY

#include <cstdint>
#include <climits>
#include <cstddef>

namespace avastd {
using int8_t   = std::int8_t;
using int16_t  = std::int16_t;
using int32_t  = std::int32_t;
using int64_t  = std::int64_t;
using uint8_t  = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using uint64_t = std::uint64_t;
using size_t   = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using intptr_t  = std::intptr_t;
using uintptr_t = std::uintptr_t;
using nullptr_t = decltype(nullptr);
constexpr size_t size_t_max = SIZE_MAX;
}  // namespace avastd

#else

namespace avastd {

using int8_t   = __INT8_TYPE__;
using int16_t  = __INT16_TYPE__;
using int32_t  = __INT32_TYPE__;
using int64_t  = __INT64_TYPE__;
using uint8_t  = __UINT8_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint32_t = __UINT32_TYPE__;
using uint64_t = __UINT64_TYPE__;
using size_t    = __SIZE_TYPE__;
using ptrdiff_t = __PTRDIFF_TYPE__;
using intptr_t  = __INTPTR_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using nullptr_t = decltype(nullptr);

// __SIZE_MAX__ es otro macro predefinido de GCC/Clang, mismo mecanismo
// que los _TYPE__ de arriba -- no requiere <stdint.h>.
constexpr size_t size_t_max = __SIZE_MAX__;

}  // namespace avastd

// El resto del codigo de AvaLang (60+ archivos en src/vm, src/builtins,
// etc., migrados o por migrar en fases siguientes) fue escrito asumiendo
// que size_t/uint8_t/int64_t/etc. estan disponibles SIN calificar --
// exactamente lo que <stddef.h>/<stdint.h> normalmente garantizan al
// inyectarlos en el namespace global. Reescribir cada uso suelto en todo
// el codebase (cientos de ocurrencias) para agregar el prefijo avastd::
// seria un cambio enorme y fragil solo para evitar esto. En cambio, se
// replica el mismo efecto que esos headers tenian: estos typedefs quedan
// disponibles TANTO en avastd:: (para codigo nuevo que prefiera ser
// explicito) COMO en el namespace global (para no tener que tocar decadas
// de codigo existente). Es exactamente lo que ya hacia el proyecto antes
// de este cambio, solo que ahora la fuente es el compilador en vez de una
// libc que no existe en este target.
using int8_t   = avastd::int8_t;
using int16_t  = avastd::int16_t;
using int32_t  = avastd::int32_t;
using int64_t  = avastd::int64_t;
using uint8_t  = avastd::uint8_t;
using uint16_t = avastd::uint16_t;
using uint32_t = avastd::uint32_t;
using uint64_t = avastd::uint64_t;
using size_t    = avastd::size_t;
using ptrdiff_t = avastd::ptrdiff_t;
using intptr_t  = avastd::intptr_t;
using uintptr_t = avastd::uintptr_t;

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_TYPES_H

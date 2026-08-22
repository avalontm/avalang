#ifndef AVA_STDCOMPAT_H
#define AVA_STDCOMPAT_H

// Punto de entrada unico a la capa de compatibilidad `avastd`.
//
// Uso: en vez de `#include <vector>` + `std::vector`, hacer
// `#include ".../stdcompat/ava_stdcompat.h"` + `avastd::vector`.
//
// Con CKM_CAP_LIBSTDCPP=1 (Windows/Linux/macOS, o cualquier target que
// termine portando libstdc++ real) todo esto es un alias directo a la STL
// real -- cero costo, cero comportamiento distinto. Con
// CKM_CAP_LIBSTDCPP=0 (litekernel hoy) son implementaciones freestanding
// propias que cubren la superficie que AvaLang usa.
//
// Ver docs/kernel/PLAN_BAREKERNEL_STDCOMPAT.md para el diseño completo,
// las fases pendientes, y el razonamiento detras de la decision.

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_new.h"
#include "ava_utility.h"
#include "ava_atomic.h"
#include "ava_vector.h"
#include "ava_string.h"
#include "ava_error.h"
#include "ava_shared_ptr.h"
#include "ava_unordered_map.h"
#include "ava_function.h"
#include "ava_mutex.h"
#include "ava_math.h"
#include "ava_algorithm.h"
#include "ava_sstream.h"
#include "ava_set.h"
#include "ava_cstdio.h"
#include "ava_error.h"

#endif  // AVA_STDCOMPAT_H

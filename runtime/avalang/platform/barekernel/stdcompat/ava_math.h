#ifndef AVA_STDCOMPAT_MATH_H
#define AVA_STDCOMPAT_MATH_H

#include "ava_platform_caps.h"

// <cmath> es un wrapper de C++ (libstdc++) sobre <math.h>. Sin libstdc++
// no hay <cmath>, pero las operaciones que AvaLang realmente usa (abs,
// round, y las que necesite vm_arith.cpp en fases siguientes: floor,
// ceil, sqrt, pow, fmod) son builtins del COMPILADOR mismo
// (__builtin_fabs, __builtin_round, etc. -- GCC/Clang los implementan sin
// necesitar ninguna libreria, bajan directo a instrucciones de FPU/SSE
// cuando es posible). Por eso avastd:: las define envolviendo esos
// builtins en vez de depender de <math.h>/libm, que en un kernel bare
// metal puede no estar linkeada.
//
// Con libstdc++ real, se usan las de <cmath> tal cual (mismo resultado,
// probablemente incluso el mismo codigo generado ya que <cmath> tambien
// suele llamar a estos builtins internamente para -O2/-O3).

#if AVA_HAVE_STD_LIBRARY

#include <cmath>
namespace avastd {
using std::abs;
using std::round;
using std::floor;
using std::ceil;
using std::sqrt;
using std::pow;
using std::fmod;
using std::isnan;
using std::isinf;
using std::fabs;
using std::trunc;
}

#else

namespace avastd {
inline double abs(double x) noexcept { return __builtin_fabs(x); }
inline double round(double x) noexcept { return __builtin_round(x); }
inline double floor(double x) noexcept { return __builtin_floor(x); }
inline double ceil(double x) noexcept { return __builtin_ceil(x); }
inline double sqrt(double x) noexcept { return __builtin_sqrt(x); }
inline double fabs(double x) noexcept { return __builtin_fabs(x); }
inline double trunc(double x) noexcept { return __builtin_trunc(x); }
// ATENCION al portar mas codigo en Fase 1+: fabs/round/floor/ceil/sqrt
// bajan a instrucciones de FPU/SSE sin necesitar ninguna libreria (el
// compilador las "constant-folds" o las emite inline). pow() y fmod(),
// en cambio, NO tienen instruccion de CPU equivalente -- __builtin_pow/
// __builtin_fmod, cuando no pueden resolverse en tiempo de compilacion,
// generan una LLAMADA a los simbolos `pow`/`fmod` de libm. Este kernel no
// linkea libm (freestanding, sin libc). Si el linker se queja de
// "undefined reference to `pow'" o `fmod'` al usar estas dos, hace falta
// implementarlas a mano (pow via exp/log si sqrt/logaritmos alcanzan
// para los casos que use AvaLang, o portar una libm minima) -- no asumir
// que compilan y listo solo porque compilan.
inline double pow(double base, double exp) noexcept { return __builtin_pow(base, exp); }
inline double fmod(double x, double y) noexcept { return __builtin_fmod(x, y); }
inline bool isnan(double x) noexcept { return __builtin_isnan(x); }
inline bool isinf(double x) noexcept { return __builtin_isinf(x); }
}

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_MATH_H

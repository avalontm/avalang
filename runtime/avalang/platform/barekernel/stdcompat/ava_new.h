#ifndef AVA_STDCOMPAT_NEW_H
#define AVA_STDCOMPAT_NEW_H

#include "ava_platform_caps.h"
#include "ava_types.h"

// operator new/delete "normales" (los que reservan memoria) son
// "language support routines": el compilador siempre espera que existan
// en algun lado con enlazado C++ estandar, con o sin libstdc++. El kernel
// ya las provee en corlib/src/cxx_runtime.cpp (via CorLib::malloc/free) --
// este header NO las vuelve a declarar/definir, seria un choque en el link.
//
// Lo que SI falta sin libstdc++ es <new>, que declara el "placement new"
// (construir un objeto en memoria ya reservada, sin pedir memoria nueva).
// avastd::vector/shared_ptr/etc. lo usan todo el tiempo. Placement new es
// parte del lenguaje -- no requiere runtime -- asi que declararlo a mano
// es seguro y es exactamente lo que hace <new> real.

#if AVA_HAVE_STD_LIBRARY

#include <new>

#else

inline void* operator new(avastd::size_t, void* ptr) noexcept { return ptr; }
inline void* operator new[](avastd::size_t, void* ptr) noexcept { return ptr; }
inline void  operator delete(void*, void*) noexcept {}
inline void  operator delete[](void*, void*) noexcept {}

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_NEW_H

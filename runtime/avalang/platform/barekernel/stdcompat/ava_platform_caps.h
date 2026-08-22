#ifndef AVA_STDCOMPAT_PLATFORM_CAPS_H
#define AVA_STDCOMPAT_PLATFORM_CAPS_H

// Traduce los capability flags del CKM (Contrato Kernel Minimo, ver
// docs/kernel/binding-status.md) a los switches que usa avastd/. Vive
// separado de BareKernelCaps.h porque stdcompat/ tiene que poder incluirse
// tambien desde builds que NO son AVA_TARGET_BAREKERNEL (Windows/Linux/
// macOS) -- ahi no existen los CKM_CAP_*, y todo se resuelve a "tengo la
// STL real".

#if defined(AVA_TARGET_BAREKERNEL) || defined(AVA_BAREKERNEL_TARGET_BINDING)
  #include "../BareKernelCaps.h"
  #define AVA_HAVE_STD_LIBRARY  CKM_CAP_LIBSTDCPP
  #define AVA_HAVE_EXCEPTIONS   CKM_CAP_STD_EXCEPTIONS
  #define AVA_HAVE_MUTEX        CKM_CAP_MUTEX
  #define AVA_HAVE_THREADS      CKM_CAP_THREADS
#else
  #define AVA_HAVE_STD_LIBRARY  1
  #define AVA_HAVE_EXCEPTIONS   1
  #define AVA_HAVE_MUTEX        1
  #define AVA_HAVE_THREADS      1
#endif

#endif  // AVA_STDCOMPAT_PLATFORM_CAPS_H

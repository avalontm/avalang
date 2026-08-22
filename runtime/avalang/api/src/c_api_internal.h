#ifndef AVA_API_C_API_INTERNAL_H
#define AVA_API_C_API_INTERNAL_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "../../platform/AvaMemory.h"

// DupString/el resto de c_api.cpp usaban std::malloc/std::free/std::memcpy
// via <cstdlib>/<cstring> -- mismo problema que ya se resolvio en
// proto_io.cpp (ver ese archivo y ava_string.h): esos headers de C++ no
// existen en este toolchain --without-headers. La correccion aca no es
// solo cambiar el include: el propio plan (Seccion 5, AvaMemory) pide que
// nada fuera de la capa de plataforma llame malloc/free directo, y todo
// el contrato publico de estos buffers (ava_module_serialize,
// ava_string_free, etc. -- ver avalang.h) ya documenta "liberar con
// ava_string_free" como el UNICO liberador esperado. Rutear
// alloc/copy/free por ava_alloc/avastd::memcpy/ava_free dej a todo el
// ciclo de vida consistente con una sola capa (ademas de ser lo unico que
// existe sin libc): hosted, ava_alloc/ava_free ya SON malloc/free (ver
// platform/linux/LinMemory.cpp), asi que el comportamiento no cambia ahi;
// en BareKernel, ava_alloc/ava_free resuelven a ckm_malloc/ckm_free.
inline char* DupString(const avastd::string& s) {
    char* out = static_cast<char*>(ava_alloc(s.size() + 1));
    avastd::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

#endif // AVA_API_C_API_INTERNAL_H

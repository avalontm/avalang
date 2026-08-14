#ifndef AVA_VM_VM_EXTERN_H
#define AVA_VM_VM_EXTERN_H

#include <string>

#include "../../api/include/avalang.h"

namespace ava {

// Metadata de una función declarada dentro de un bloque `extern`.
// Ver EXTERN_FFI_DESIGN.md / EXTERN_FFI_TODO.md (Fase 3).
struct ExternFuncMeta {
    std::string library;     // nombre lógico, p.ej. "kernel32"
    std::string alias;       // p.ej. "Kernel"
    std::string func_name;   // p.ej. "Sleep"
    size_t arity = 0;
    bool is_vararg = false;
};

// Native function real de una llamada `Alias.Func(...)`. Carga la
// librería nativa (cacheada por nombre lógico), resuelve el símbolo
// (cacheado en `meta`) y lo invoca con libffi.
//
// Limitaciones conocidas (ver EXTERN_FFI_TODO.md):
// - Argumentos soportados: number (int64 si es entero exacto, si no
//   double), string (const char*), bool (int64 0/1), nil (puntero NULL).
//   List/Dict/Instance/etc. no son soportados todavía.
// - Valor de retorno: siempre se interpreta como int64 (cubre void/bool/
//   int, que son la mayoría de las funciones de la Win32 API como
//   Sleep/Beep). Retornos double/float van a leer el registro
//   equivocado -- esto requiere firmas tipadas (`func F(x:Int32)`),
//   fuera de alcance del diseño actual.
// - Convención de llamada: se usa FFI_DEFAULT_ABI. En x64 (Windows/Linux/
//   macOS) stdcall y cdecl son la misma ABI, así que esto no es un
//   problema para builds x64 (que es el target de este proyecto,
//   vcpkg triplet x64-windows-static-md). No probado en x86/ARM.
// - En Windows, la llamada nativa corre bajo SEH (__try/__except): si un
//   extern mal declarado (argumentos que no calzan con la firma real de
//   C) hace que la DLL lea/escriba memoria invalida, eso ya no mata el
//   proceso en silencio -- se convierte en un runtime_error normal que
//   nombra la funcion que crasheo. No linux/macOS todavia (requeriria
//   signal handlers en vez de SEH).
extern "C" ava_value_t ava_extern_call(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data);

} // namespace ava

#endif // AVA_VM_VM_EXTERN_H

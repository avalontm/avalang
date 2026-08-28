#include "builtin.h"
#include "builtin_natives.h"
#include "builtin_names.h"
#include "vm/vm.h"

using namespace ava;

extern "C" {

// Declared in builtin.h, called from public/src/c_api.cpp's
// ava_vm_create(). Registers every global (bare-name) native — as
// opposed to RegisterBuiltinMethods, which registers dotted methods
// like str.upper(). `str`, `len`, and `type` are load-bearing for
// core/src/compiler/compiler.cpp (f-string joins, for-loops, dynamic-for
// dispatch).
//
// Fase 1 de PLAN_VALIDACION_ESTATICA.md: el listado de nombres ya no
// vive acá — viene de AVA_BUILTIN_GLOBALS en builtin_names.h, la misma
// lista que Compiler::CheckCallArgs consulta para saber si un nombre no
// resuelto es un builtin válido antes de reportar "function is not
// defined". Incluye también coroutine/resume/set_timeout/sleep_async/
// clear_timeout/delay, que antes se registraban sueltos en
// builtin_registry.cpp — movidos acá porque también son bare natives,
// no métodos dotted, y por lo tanto pertenecen a esta misma lista.
BUILTIN_API void RegisterBuiltinGlobals(AvaVM* vm) {
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);

#define AVA_REGISTER_GLOBAL(name, fn) raw_vm->RegisterNative(#name, fn, nullptr);
    AVA_BUILTIN_GLOBALS(AVA_REGISTER_GLOBAL)
#undef AVA_REGISTER_GLOBAL
}

// Declared in builtin.h but (per grep across this snapshot) never
// actually called from anywhere — ava_vm_create() calls
// RegisterBuiltinMethods + RegisterBuiltinGlobals directly, not this.
// Kept as a convenience entry point that does both, for any external
// embedder that wants a single call.
BUILTIN_API void RegisterAll(AvaVM* vm) {
    RegisterBuiltinMethods(vm);
    RegisterBuiltinGlobals(vm);
}

} // extern "C"

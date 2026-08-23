// main_barekernel.cpp -- Fase B1 de plan_avapack_barekernel.md.
//
// Es un fork deliberado de runtime/avabare/src/ava_barekernel_runner.cpp:
// mismo _start freestanding, mismo dlopen+dlsym de libavalang.so, misma
// falta de heap propio (bootstrap corre antes de que la PAL de AvaLang
// esté disponible) -- la única diferencia real es de dónde sale el
// bytecode .avb:
//
//   avabare (avabare/ hoy):        ckm_open("/apps/hello.avb") + ckm_read
//   avapack_barekernel (este archivo): kAvbBytes/kAvbSize, ya en el .rodata
//                                       del propio binario (embedded_avb.h,
//                                       generado por avapack_barekernel_gen)
//
// Esto es justo lo que el plan identifica como lo único que falta para que
// una app AvaLang "se sienta como una app normal" (un solo .exe, sin un
// .avb suelto al lado que copiar aparte a /apps/) -- ver plan §3.
//
// No incluye avastd, STL, ni el resto de la PAL por el mismo motivo que
// avabare: corre *antes* de que libavalang.so esté cargado, así que no
// puede depender de símbolos que viven adentro de esa librería. Usa
// ckm_contract.h/ckm_syscall.h directamente, igual que el original.
//
// Validado (por ahora): -fsyntax-only, mismo criterio que avabare/README.md
// (ver Fase B1 del plan -- no ejecución real todavía, eso es Fase B3).

#include "../../../avalang/api/include/avalang.h"
#include "../../../avalang/platform/barekernel/ckm_contract.h"
#include "embedded_avb.h"

namespace {

constexpr const char* kLibPath = "/system/lib/libavalang.so";

typedef AvaVM*     (*AvaVmCreateFn)(void);
typedef void       (*AvaVmDestroyFn)(AvaVM*);
typedef AvaModule* (*AvaModuleDeserializeFn)(AvaVM*, const uint8_t*, size_t, char**);
typedef void       (*AvaModuleDestroyFn)(AvaModule*);
typedef void       (*AvaRunFn)(AvaVM*, AvaModule*, ava_value_t*, char**);
typedef void       (*AvaStringFreeFn)(char*);

size_t RawStrLen(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

void WriteOut(int fd, const char* s) {
    ckm_write(fd, s, static_cast<long>(RawStrLen(s)));
}

} // namespace

extern "C" void _start() {
    // Sin heap propio en este punto (igual que avabare) -- el .avb ya
    // vive como dato estático embebido en el binario (kAvbBytes), no hace
    // falta un buffer donde leerlo desde disco.
    void* handle = ckm_dlopen(kLibPath, CKM_RTLD_NOW);
    if (!handle) {
        WriteOut(CKM_STDERR, "avapack_barekernel: dlopen fallo para libavalang.so\n");
        ckm_exit(1);
    }

    auto vm_create = reinterpret_cast<AvaVmCreateFn>(ckm_dlsym(handle, "ava_vm_create"));
    auto vm_destroy = reinterpret_cast<AvaVmDestroyFn>(ckm_dlsym(handle, "ava_vm_destroy"));
    auto module_deserialize = reinterpret_cast<AvaModuleDeserializeFn>(ckm_dlsym(handle, "ava_module_deserialize"));
    auto module_destroy = reinterpret_cast<AvaModuleDestroyFn>(ckm_dlsym(handle, "ava_module_destroy"));
    auto run = reinterpret_cast<AvaRunFn>(ckm_dlsym(handle, "ava_run"));
    auto string_free = reinterpret_cast<AvaStringFreeFn>(ckm_dlsym(handle, "ava_string_free"));

    if (!vm_create || !vm_destroy || !module_deserialize || !module_destroy || !run || !string_free) {
        WriteOut(CKM_STDERR, "avapack_barekernel: dlsym fallo resolviendo la C API\n");
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    AvaVM* vm = vm_create();
    if (!vm) {
        WriteOut(CKM_STDERR, "avapack_barekernel: ava_vm_create fallo\n");
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    // A diferencia de avabare (lee bytes de /apps/hello.avb a un buffer),
    // acá el módulo ya está en el binario -- ava_module_deserialize lee
    // directo de kAvbBytes/kAvbSize (.rodata, no hace falta copiarlo).
    char* error = nullptr;
    AvaModule* module = module_deserialize(
        vm,
        avapack_barekernel::kAvbBytes,
        avapack_barekernel::kAvbSize,
        &error);
    if (!module) {
        WriteOut(CKM_STDERR, "avapack_barekernel: bytecode embebido invalido (");
        WriteOut(CKM_STDERR, avapack_barekernel::kEntryName);
        WriteOut(CKM_STDERR, "): ");
        if (error) { WriteOut(CKM_STDERR, error); string_free(error); }
        WriteOut(CKM_STDERR, "\n");
        vm_destroy(vm);
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    ava_value_t result;
    error = nullptr;
    run(vm, module, &result, &error);
    if (error) {
        WriteOut(CKM_STDERR, "avapack_barekernel: error en ejecucion: ");
        WriteOut(CKM_STDERR, error);
        WriteOut(CKM_STDERR, "\n");
        string_free(error);
        // Ver comentario en ava_barekernel_runner.cpp: orden invertido
        // para que el VM suelte sus referencias compartidas antes de que
        // module_destroy libere el Proto (evita el doble-free en teardown).
        vm_destroy(vm);
        module_destroy(module);
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    vm_destroy(vm);
    module_destroy(module);
    ckm_dlclose(handle);
    ckm_exit(0);
}

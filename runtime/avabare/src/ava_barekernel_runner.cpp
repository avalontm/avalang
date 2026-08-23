#include "../../avalang/api/include/avalang.h"
#include "../../avalang/platform/barekernel/ckm_contract.h"

namespace {

constexpr const char* kLibPath = "/system/lib/libavalang.so";
constexpr const char* kBytecodePath = "/apps/hello.avb";
constexpr size_t kReadChunk = 4096;
constexpr size_t kMaxBytecodeSize = 1u * 1024u * 1024u;

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

bool ReadWholeFile(const char* path, uint8_t* buf, size_t buf_cap, size_t* out_len) {
    int fd = ckm_open(path, CKM_O_RDONLY, 0);
    if (fd < 0) return false;

    size_t total = 0;
    for (;;) {
        if (total >= buf_cap) { ckm_close(fd); return false; }
        long remaining = static_cast<long>(buf_cap - total);
        long to_read = remaining < static_cast<long>(kReadChunk) ? remaining : static_cast<long>(kReadChunk);
        long n = ckm_read(fd, buf + total, to_read);
        if (n < 0) { ckm_close(fd); return false; }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    ckm_close(fd);
    *out_len = total;
    return true;
}

}

extern "C" void _start() {
    static uint8_t bytecode_buf[kMaxBytecodeSize];

    void* handle = ckm_dlopen(kLibPath, CKM_RTLD_NOW);
    if (!handle) {
        WriteOut(CKM_STDERR, "ava_barekernel_runner: dlopen fallo para libavalang.so\n");
        ckm_exit(1);
    }

    auto vm_create = reinterpret_cast<AvaVmCreateFn>(ckm_dlsym(handle, "ava_vm_create"));
    auto vm_destroy = reinterpret_cast<AvaVmDestroyFn>(ckm_dlsym(handle, "ava_vm_destroy"));
    auto module_deserialize = reinterpret_cast<AvaModuleDeserializeFn>(ckm_dlsym(handle, "ava_module_deserialize"));
    auto module_destroy = reinterpret_cast<AvaModuleDestroyFn>(ckm_dlsym(handle, "ava_module_destroy"));
    auto run = reinterpret_cast<AvaRunFn>(ckm_dlsym(handle, "ava_run"));
    auto string_free = reinterpret_cast<AvaStringFreeFn>(ckm_dlsym(handle, "ava_string_free"));

    if (!vm_create || !vm_destroy || !module_deserialize || !module_destroy || !run || !string_free) {
        WriteOut(CKM_STDERR, "ava_barekernel_runner: dlsym fallo resolviendo la C API\n");
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    size_t bytecode_len = 0;
    if (!ReadWholeFile(kBytecodePath, bytecode_buf, sizeof(bytecode_buf), &bytecode_len)) {
        WriteOut(CKM_STDERR, "ava_barekernel_runner: no se pudo leer el bytecode\n");
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    AvaVM* vm = vm_create();
    if (!vm) {
        WriteOut(CKM_STDERR, "ava_barekernel_runner: ava_vm_create fallo\n");
        ckm_dlclose(handle);
        ckm_exit(1);
    }

    char* error = nullptr;
    AvaModule* module = module_deserialize(vm, bytecode_buf, bytecode_len, &error);
    if (!module) {
        WriteOut(CKM_STDERR, "ava_barekernel_runner: bytecode invalido: ");
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
        WriteOut(CKM_STDERR, "ava_barekernel_runner: error en ejecucion: ");
        WriteOut(CKM_STDERR, error);
        WriteOut(CKM_STDERR, "\n");
        string_free(error);
        // Orden invertido a proposito: el Proto del modulo (via
        // avastd::shared_ptr<Proto>) y el VM pueden tener Values
        // compartidos apuntando al mismo Object del GC (constantes de
        // string, etc.). Destruir el VM primero permite que suelte sus
        // propias referencias mientras el Proto/modulo todavia esta
        // vivo, evitando que module_destroy libere un Object que el
        // shutdown del VM todavia espera poder tocar.
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

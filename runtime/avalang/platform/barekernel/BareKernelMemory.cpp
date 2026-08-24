#include "../AvaMemory.h"
#include "ckm_contract.h"
#include "stdcompat/ava_platform_caps.h"
#include "stdcompat/ava_types.h"

namespace {

// alignas(16): sin esto, sizeof(AvaAllocHeader) es sizeof(size_t) == 4
// bytes en este target (i686, 32 bits) -- y como el puntero que se
// devuelve al llamador es exactamente `raw + sizeof(AvaAllocHeader)`,
// cualquier alineacion que ckm_malloc(total) haya garantizado sobre
// `raw` queda corrida 4 bytes en el puntero de usuario. operator new
// esta obligado por el standard a devolver memoria alineada a
// __STDCPP_DEFAULT_NEW_ALIGNMENT__ (16 en x86, cubre double/int64_t/
// SSE), y ese es justamente el unico consumidor real de ava_alloc en
// este target (ver ava_new.h). Con el header paddeado a 16 bytes, el
// corrimiento es un multiplo exacto de 16 y no rompe nada que
// ckm_malloc ya haya alineado; no depende de conocer el alineamiento
// exacto que da ckm_malloc, solo de que sea <= 16 (cualquier
// allocator de proposito general cumple esto).
struct alignas(16) AvaAllocHeader {
    size_t size;
};

}

extern "C" {

void* ava_alloc(size_t size) {
    size_t total = size + sizeof(AvaAllocHeader);
    void* raw = ckm_malloc(total);
    if (!raw) return nullptr;
    AvaAllocHeader* header = static_cast<AvaAllocHeader*>(raw);
    header->size = size;
    return static_cast<void*>(header + 1);
}

void* ava_realloc(void* ptr, size_t size) {
    if (!ptr) return ava_alloc(size);
    if (size == 0) {
        ava_free(ptr);
        return nullptr;
    }
    AvaAllocHeader* header = static_cast<AvaAllocHeader*>(ptr) - 1;
    size_t old_size = header->size;
    void* new_ptr = ava_alloc(size);
    if (!new_ptr) return nullptr;
    size_t copy = old_size < size ? old_size : size;
    unsigned char* src = static_cast<unsigned char*>(ptr);
    unsigned char* dst = static_cast<unsigned char*>(new_ptr);
    for (size_t i = 0; i < copy; ++i) dst[i] = src[i];
    ava_free(ptr);
    return new_ptr;
}

void ava_free(void* ptr) {
    if (!ptr) return;
    AvaAllocHeader* header = static_cast<AvaAllocHeader*>(ptr) - 1;
    ckm_free(static_cast<void*>(header));
}

}

// operator new/delete "normales" (no placement): sin libstdc++ el
// compilador igual los necesita como "language support routines" para
// cualquier `new`/`delete` que use el codigo (avastd::shared_ptr,
// vectores, etc). Se definen ACA -- adentro del propio binario de
// avalang -- y no se asume que el kernel los provea, porque
// LibraryLoader::ApplyRelocation (lib_loader.cpp del kernel) resuelve
// simbolos unicamente contra el symtab del propio .so, nunca contra
// otras bibliotecas ya cargadas. Ver ava_new.h para las declaraciones
// y el razonamiento completo.
#if !AVA_HAVE_STD_LIBRARY

namespace {
// Diagnostico temporal: sin excepciones, si ckm_malloc falla,
// ava_alloc devuelve nullptr y el new-expression sigue construyendo
// el objeto en la direccion 0 sin avisar -- eso encajaria exactamente
// con un vtable read en address 0 mas adelante. Este log lo hace visible.
void DebugWriteAllocFail(avastd::size_t size) {
    ckm_write(CKM_STDOUT, "[AVALANG] operator new(", 23);
    char digits[24];
    int n = 0;
    unsigned long s = (unsigned long)size;
    if (s == 0) { digits[n++] = '0'; }
    while (s > 0) { digits[n++] = char('0' + (s % 10)); s /= 10; }
    while (n > 0) { ckm_write(CKM_STDOUT, &digits[--n], 1); }
    ckm_write(CKM_STDOUT, ") -> ava_alloc devolvio NULL\n", 29);
}
}

void* operator new(avastd::size_t size) {
    void* p = ava_alloc(size ? size : 1);
    if (!p) DebugWriteAllocFail(size);
    return p;
}

void* operator new[](avastd::size_t size) {
    void* p = ava_alloc(size ? size : 1);
    if (!p) DebugWriteAllocFail(size);
    return p;
}

void operator delete(void* ptr) noexcept {
    ava_free(ptr);
}

void operator delete[](void* ptr) noexcept {
    ava_free(ptr);
}

void operator delete(void* ptr, avastd::size_t) noexcept {
    ava_free(ptr);
}

void operator delete[](void* ptr, avastd::size_t) noexcept {
    ava_free(ptr);
}

#endif  // !AVA_HAVE_STD_LIBRARY

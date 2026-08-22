#include "../AvaMemory.h"
#include "ckm_contract.h"

namespace {

struct AvaAllocHeader {
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

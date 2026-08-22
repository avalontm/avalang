#include "../AvaMemory.h"
#include <cstdlib>

extern "C" {

void* ava_alloc(size_t size) {
    return std::malloc(size);
}

void* ava_realloc(void* ptr, size_t size) {
    return std::realloc(ptr, size);
}

void ava_free(void* ptr) {
    std::free(ptr);
}

}

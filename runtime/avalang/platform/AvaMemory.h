#ifndef AVA_PLATFORM_MEMORY_H
#define AVA_PLATFORM_MEMORY_H

#include <stddef.h>

extern "C" {

void* ava_alloc(size_t size);
void* ava_realloc(void* ptr, size_t size);
void  ava_free(void* ptr);

}

#endif // AVA_PLATFORM_MEMORY_H

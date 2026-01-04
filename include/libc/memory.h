#ifndef MEMORY_H
#define MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"

void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

void heapinit(uint8_t* start, uint8_t* end);
bool heap_can_alloc(size_t size); // check if size can be allocated to heap (used internally by malloc, zalloc etc)

void* malloc(size_t size);
void* zalloc(size_t size);
void free(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_H
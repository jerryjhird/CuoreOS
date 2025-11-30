#ifndef KMEM_H
#define KMEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

// init heap
void kheapinit(uint8_t* start, uint8_t* end);

// allocate memory
void* kmalloc(size_t size);
void* kzalloc(size_t size);

// free memory
void kfree(void* ptr, size_t size);

// convert ptr to hex
#ifndef ptrhex
void *ptrhex(char *buf, void *ptr);
#endif

#ifdef __cplusplus
}
#endif

#endif // KMEM_H
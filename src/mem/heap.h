#pragma once

#include <stddef.h>

void heap_init(void *start_address, size_t total_size);
void dump_memory_stats(void);

void *malloc(size_t size); // allocate memory
void *zalloc(size_t size); // allocate memory and zero it

void free(void *ptr); // free a pointer
void sfree(void *ptr, size_t size);
void zfree(void *ptr); // free a pointer and zero the memory
void szfree(void *ptr, size_t size);

void *realloc(void *ptr, size_t new_size);

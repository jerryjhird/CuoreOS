#pragma once

#include <stddef.h>
#include <stdint.h>

#define HEAP_ALIGNMENT 16
#define HEAP_MAGIC 0xCAFEBABE
#define MIN_SPLIT_SIZE 16
#define PAGES_PER_GROWTH 4
#define KERNEL_HEAP_FLAGS (PTE_PRESENT | PTE_WRITABLE | PTE_NX)

void heap_init(void* start_address, size_t total_size);
void* malloc(size_t size); // allocate memory
void* realloc(void* ptr, size_t new_size);
void* zalloc(size_t size); // allocate memory and zero it
void free(void* ptr); // free a pointer
void sfree(void* ptr, size_t size);
void zfree(void* ptr); // free a pointer and zero the memory
void szfree(void* ptr, size_t size);

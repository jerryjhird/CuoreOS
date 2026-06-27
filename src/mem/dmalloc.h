#pragma once

// direct memory allocator

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct DMARegistry {
	uintptr_t virt;
	size_t pages;
	struct DMARegistry *next;
} DMARegistry;

typedef struct {
	uint64_t virt;
	uint64_t phys;
} dmalloc_ret_t;

dmalloc_ret_t dmalloc(size_t size);
dmalloc_ret_t dmalloc32(size_t size);
void dmfree(uint64_t virt_addr);

#pragma once

// direct memory allocator

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct DMABlock {
	uint32_t magic;
	size_t size;
	uintptr_t phys_addr;
	bool is_free;
	struct DMABlock *next;
	struct DMABlock *prev;
} DMABlock;

typedef struct {
	uint64_t virt;
	uint64_t phys;
} dmalloc_ret_t;

#define DMA_MAGIC 0xDA7A0000

dmalloc_ret_t dmalloc(size_t size);
dmalloc_ret_t dmalloc32(size_t size);
void dmfree(uint64_t virt_addr);

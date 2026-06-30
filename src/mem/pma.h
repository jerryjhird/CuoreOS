#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sync.h"

#define PMA_ZONE_32BIT (0x100000000ULL / PAGE_SIZE)

#define PMA_FL_INDEX_COUNT 32
#define PMA_SL_INDEX_COUNT 32

// TLSF constants, minimum block is page-aligned
#define PMA_ALIGN_SIZE_LOG2 12
#define PMA_ALIGN_SIZE (1ULL << PMA_ALIGN_SIZE_LOG2)
#define PMA_SL_INDEX_COUNT_LOG2 5
#define PMA_FL_INDEX_SHIFT (PMA_SL_INDEX_COUNT_LOG2 + PMA_ALIGN_SIZE_LOG2)
#define PMA_SMALL_BLOCK_SIZE (1ULL << PMA_FL_INDEX_SHIFT)
#define PMA_FL_INDEX_COUNT 32
#define PMA_BLOCK_HEADER_SIZE sizeof(pma_free_block_t)

typedef struct pma_free_block {
	struct pma_free_block *prev;
	struct pma_free_block *next;
	size_t size;
} pma_free_block_t;

typedef struct pma_zone {
	spinlock_t lock;
	const char *name;
	uintptr_t base;
	size_t total_pages;
	uint8_t *bitmap;
	uint64_t fl_bitmap;
	uint64_t sl_bitmap[PMA_FL_INDEX_COUNT];
	pma_free_block_t *blocks[PMA_FL_INDEX_COUNT][PMA_SL_INDEX_COUNT];
	pma_free_block_t block_null;
} pma_zone_t;

extern pma_zone_t pma_normal;
extern pma_zone_t pma_low;

void pma_init(void);

uintptr_t pma_alloc_pages(size_t count);
uintptr_t pma_alloc_pages_2mb(size_t count);
uintptr_t pma_alloc_pages_low(size_t count);

void pma_free_pages(uintptr_t phys, size_t count);
void pma_free_page(uintptr_t phys);

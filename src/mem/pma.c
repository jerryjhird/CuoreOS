#include "pma.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "mem.h"
#include "abs.h"
#include "datatype/bitmap.h"
#include "panic.h"
#include "cmm.h"

static uint8_t *pma_bitmap = NULL;
static size_t pma_total_pages = 0;
static size_t pma_bitmap_bytes = 0;
static size_t pma_free_pages_count = 0;

void pma_init(void) {
	pma_free_pages_count = 0;

	pma_total_pages = cmm_get_ram_top() / PAGE_SIZE;
	pma_bitmap_bytes = (pma_total_pages + 7) / 8;
	size_t bitmap_pages = (pma_bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
	size_t bitmap_reserved_bytes = bitmap_pages * PAGE_SIZE;

	uintptr_t bitmap_phys_addr = 0;
	for (size_t i = 0; i < cmm_get_region_count(); i++) {
		const struct cmm_region *r = cmm_get_region(i);
		if (r->type == CMM_USABLE && r->length >= bitmap_reserved_bytes) {
			bitmap_phys_addr = r->base;
			pma_bitmap = (uint8_t *)(bitmap_phys_addr + hhdm_offset);
			memset(pma_bitmap, 0xFF, pma_bitmap_bytes);
			break;
		}
	}

	if (!pma_bitmap) panic("PMA", "failed to allocate bitmap");

	for (size_t i = 0; i < cmm_get_region_count(); i++) {
		const struct cmm_region *r = cmm_get_region(i);

		if (r->type == CMM_USABLE) {
			for (uintptr_t addr = r->base; addr < r->base + r->length; addr += PAGE_SIZE) {
				if (addr >= bitmap_phys_addr && addr < bitmap_phys_addr + bitmap_reserved_bytes) {
					continue;
				}

				size_t page_idx = addr / PAGE_SIZE;
				if (page_idx < pma_total_pages) {
					BITMAP_CLEAR(pma_bitmap, page_idx);
					pma_free_pages_count++;
				}
			}
		}
	}

	BITMAP_SET(pma_bitmap, 0);
}

static uintptr_t _pma_alloc_range(size_t count, size_t limit) {
	if (UNLIKELY(count == 0)) return 0;
	if (limit > pma_total_pages) limit = pma_total_pages;

	size_t search_idx = 0;
	while (search_idx < limit) {
		size_t next_free = bitmap_find_next_free(pma_bitmap, search_idx, limit);

		if (next_free == (size_t)-1) break; // OOM
		search_idx = next_free;

		size_t consecutive = 0;
		for (size_t i = 0; i < count; i++) {
			if (search_idx + i < limit && !BITMAP_TEST(pma_bitmap, search_idx + i)) {
				consecutive++;
			} else {
				break;
			}
		}

		if (consecutive == count) {
			for (size_t j = 0; j < count; j++) {
				BITMAP_SET(pma_bitmap, search_idx + j);
			}
			return (uintptr_t)(search_idx * PAGE_SIZE);
		}

		search_idx++;
	}

	return 0; // OOM
}

// allocate anywhere in physical memory
uintptr_t pma_alloc_pages(size_t count) {
	uintptr_t addr = _pma_alloc_range(count, pma_total_pages);
	if (UNLIKELY(addr == 0)) {
		panic("PMA", "Out of memory");
	}
	return addr;
}

// below 4gb memory allocation for legacy devices that cant do 64 bit DMA
uintptr_t pma_alloc_pages_low(size_t count) {
	uintptr_t addr = _pma_alloc_range(count, PMA_ZONE_32BIT);
	if (UNLIKELY(addr == 0)) {
		panic("PMA", "Out of memory in 32-bit (low) range");
	}
	return addr;
}

// free pages
void pma_free_pages(uintptr_t phys, size_t count) {
	size_t start_bit = phys / PAGE_SIZE;
	for (size_t i = 0; i < count; i++) BITMAP_CLEAR(pma_bitmap, start_bit + i);
}

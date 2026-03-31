#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "kstate.h"
#include "mem.h" // IWYU pragma: keep

#define PMA_PAGE_SIZE 4096

static uint8_t *pma_bitmap = NULL;
static size_t pma_total_pages = 0;
static size_t pma_bitmap_bytes = 0;
static size_t last_pma_index = 0; // performance hint
static size_t pma_free_pages_count = 0;

static inline void bit_set(size_t bit) {
	pma_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bit_clear(size_t bit) {
	pma_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bit_test(size_t bit) {
	return (pma_bitmap[bit / 8] >> (bit % 8)) & 1;
}

void pma_init(void) {
	uintptr_t max_phys = 0;
	pma_free_pages_count = 0;

	for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
		struct limine_memmap_entry *e = memmap_request.response->entries[i];

		if (e->type != LIMINE_MEMMAP_USABLE &&
			e->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
			continue;
		}

		uintptr_t end = e->base + e->length;
		if (end > max_phys)
			max_phys = end;
	}

	pma_total_pages = max_phys / PMA_PAGE_SIZE;
	pma_bitmap_bytes = (pma_total_pages + 7) / 8;

	size_t bitmap_pages = (pma_bitmap_bytes + PMA_PAGE_SIZE - 1) / PMA_PAGE_SIZE;
	size_t bitmap_reserved_bytes = bitmap_pages * PMA_PAGE_SIZE;

	for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
		struct limine_memmap_entry *e = memmap_request.response->entries[i];
		if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_reserved_bytes) {
			pma_bitmap = (uint8_t *)(e->base + hhdm_offset);

			memset(pma_bitmap, 0xFF, pma_bitmap_bytes);

			e->base += bitmap_reserved_bytes;
			e->length -= bitmap_reserved_bytes;
			break;
		}
	}

	// mark usable regions as free
	for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
		struct limine_memmap_entry *e = memmap_request.response->entries[i];
		if (e->type == LIMINE_MEMMAP_USABLE) {
			for (uintptr_t addr = e->base; addr < e->base + e->length; addr += PMA_PAGE_SIZE) {
				size_t page_idx = addr / PMA_PAGE_SIZE;
				if (page_idx < pma_total_pages) {
					bit_clear(page_idx);
					pma_free_pages_count++; // Increment the fast counter
				}
			}
		}
	}
}

uintptr_t pma_alloc_pages(size_t count) {
	if (count == 0)
		return 0;

	size_t consecutive_found = 0;
	size_t start_bit = 0;

	for (size_t i = last_pma_index; i < pma_total_pages + last_pma_index; i++) {
		size_t idx = i % pma_total_pages;

		if (!bit_test(idx)) {
			if (consecutive_found == 0)
				start_bit = idx;
			consecutive_found++;

			if (consecutive_found == count) {
				for (size_t j = 0; j < count; j++) {
					bit_set(start_bit + j);
				}

				last_pma_index = (start_bit + count) % pma_total_pages;
				return (uintptr_t)(start_bit * PMA_PAGE_SIZE);
			}
		} else {
			consecutive_found = 0;
		}
	}

	return 0; // OOM
}

uintptr_t pma_alloc_page(void) { return pma_alloc_pages(1); }

void pma_free_pages(uintptr_t phys, size_t count) {
	size_t start_bit = phys / PMA_PAGE_SIZE;
	for (size_t i = 0; i < count; i++)
		bit_clear(start_bit + i);
}

void pma_free_page(uintptr_t phys) { pma_free_pages(phys, 1); }

// stats

size_t pma_get_total_pages(void) {
	return pma_total_pages;
}

size_t pma_get_free_pages(void) {
	size_t free_count = 0;
	for (size_t i = 0; i < pma_total_pages; i++) {
		if (!bit_test(i)) {
			free_count++;
		}
	}
	return free_count;
}

size_t pma_get_used_pages(void) {
	return pma_total_pages - pma_get_free_pages();
}

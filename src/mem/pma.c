#include "pma.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <limine.h>
#include "mem.h"
#include "abs.h"
#include "panic.h"
#include "cmm.h"
#include "datatype/bitmap.h"
#include "sync.h"

pma_zone_t pma_normal;
pma_zone_t pma_low;

// bit manipulation
static inline int pma_ffs(uint64_t word) {
	return __builtin_ffsll((long long)word) - 1;
}

static inline int pma_fls(uint64_t word) {
	if (!word) return -1;
	return 63 - __builtin_clzll(word);
}

// block header helpers, blocks live in free physical memory via HHDM
// for a free block at phys:
//   phys + hhdm_offset -> pma_free_block_t (prev, next, size | free_bit)
// for an allocated block at phys:
//   no header, user gets the entire page-aligned range

static inline uintptr_t block_phys(const pma_free_block_t *block) {
	return (uintptr_t)block - hhdm_offset;
}

static inline pma_free_block_t *block_from_phys(uintptr_t phys) {
	return (pma_free_block_t *)(phys + hhdm_offset);
}

static inline size_t block_size(const pma_free_block_t *block) {
	return block->size & ~1ULL;
}

static inline void block_set_size(pma_free_block_t *block, size_t sz) {
	block->size = sz | (block->size & 1);
}

static inline void block_mark_used(pma_free_block_t *block) {
	block->size &= ~1ULL;
}

// TLSF mapping: size -> (fl, sl)
static void tlsf_mapping_insert(size_t size, int *fli, int *sli) {
	int fl, sl;

	if (size < PMA_SMALL_BLOCK_SIZE) {
		fl = 0;
		sl = (int)(size >> PMA_ALIGN_SIZE_LOG2);
	} else {
		fl = pma_fls((uint64_t)size);
		sl = (int)((size >> (fl - PMA_SL_INDEX_COUNT_LOG2)) ^ (1ULL << PMA_SL_INDEX_COUNT_LOG2));
		fl -= (PMA_FL_INDEX_SHIFT - 1);
	}

	*fli = fl;
	*sli = sl;
}

// TLSF free list operations
static void tlsf_remove_free_block(pma_zone_t *z, pma_free_block_t *b, int fl, int sl) {
	pma_free_block_t *prev = b->prev;
	pma_free_block_t *next = b->next;
	next->prev = prev;
	prev->next = next;

	if (z->blocks[fl][sl] == b) {
		z->blocks[fl][sl] = next;
		if (next == &z->block_null) {
			z->sl_bitmap[fl] &= ~(1ULL << sl);
			if (!z->sl_bitmap[fl]) {
				z->fl_bitmap &= ~(1ULL << fl);
			}
		}
	}
}

static void tlsf_insert_free_block(pma_zone_t *z, pma_free_block_t *b, int fl, int sl) {
	pma_free_block_t *current = z->blocks[fl][sl];
	b->next = current;
	b->prev = &z->block_null;
	current->prev = b;

	z->blocks[fl][sl] = b;
	z->fl_bitmap |= (1ULL << fl);
	z->sl_bitmap[fl] |= (1ULL << sl);
}

static void tlsf_block_remove(pma_zone_t *z, pma_free_block_t *b) {
	int fl, sl;
	tlsf_mapping_insert(block_size(b), &fl, &sl);
	tlsf_remove_free_block(z, b, fl, sl);
}

static void tlsf_block_insert(pma_zone_t *z, pma_free_block_t *b) {
	int fl, sl;
	tlsf_mapping_insert(block_size(b), &fl, &sl);
	tlsf_insert_free_block(z, b, fl, sl);
}

// search for a suitable free block
static pma_free_block_t *tlsf_search_suitable(pma_zone_t *z, int *fli, int *sli) {
	int fl = *fli;
	int sl = *sli;

	uint64_t sl_map = z->sl_bitmap[fl] & (~0ULL << sl);
	if (!sl_map) {
		uint64_t fl_map = z->fl_bitmap & (~0ULL << (fl + 1));
		if (!fl_map) return NULL;
		fl = pma_ffs(fl_map);
		*fli = fl;
		sl_map = z->sl_bitmap[fl];
	}

	sl = pma_ffs(sl_map);
	*sli = sl;
	return z->blocks[fl][sl];
}

static pma_free_block_t *tlsf_locate_free(pma_zone_t *z, size_t size) {
	int fl = 0, sl = 0;
	pma_free_block_t *block = NULL;

	if (size) {
		// use mapping_insert (not mapping_search) because our blocks
		// have no header overhead for allocated memory, the full
		// block size is usable. mapping_search rounds up, which can
		// skip blocks in lower SLs within the same FL.
		tlsf_mapping_insert(size, &fl, &sl);
		if (fl < PMA_FL_INDEX_COUNT) {
			block = tlsf_search_suitable(z, &fl, &sl);
		}
	}

	if (block) {
		tlsf_remove_free_block(z, block, fl, sl);
	}

	return block;
}

// split a free block: first `size` bytes allocated, rest stays free
// returns pointer to remaining (free) block
static pma_free_block_t *block_split(pma_free_block_t *block, size_t size) {
	uintptr_t b_phys = block_phys(block);
	uintptr_t r_phys = b_phys + size;
	pma_free_block_t *remaining = block_from_phys(r_phys);

	size_t total = block_size(block);
	size_t remain_size = total - size;

	remaining->size = remain_size | 1;
	block_set_size(block, size);

	return remaining;
}

// bitmap helpers
static inline int pma_page_is_free(pma_zone_t *z, size_t pfn) {
	if (pfn >= z->total_pages) return false;
	return !BITMAP_TEST(z->bitmap, pfn);
}

static inline void pma_page_set_free(pma_zone_t *z, size_t pfn) {
	if (pfn >= z->total_pages) return;
	BITMAP_CLEAR(z->bitmap, pfn);
}

static inline void pma_page_set_used(pma_zone_t *z, size_t pfn) {
	if (pfn >= z->total_pages) return;
	BITMAP_SET(z->bitmap, pfn);
}

// scan backward to find the first page of a free block
static size_t pma_find_free_start(pma_zone_t *z, size_t pfn) {
	while (pfn > 0 && pma_page_is_free(z, pfn - 1)) {
		pfn--;
	}
	return pfn;
}

// scan forward past all free pages starting at pfn
static size_t pma_find_free_end(pma_zone_t *z, size_t pfn) {
	while (pfn < z->total_pages && pma_page_is_free(z, pfn)) {
		pfn++;
	}
	return pfn;
}

// zone init
static void pma_zone_init(pma_zone_t *z, uintptr_t base, size_t total_pages, uint8_t *bitmap, const char *name) {
	z->lock = (spinlock_t)SPINLOCK_INIT;
	z->name = name;
	z->base = base;
	z->total_pages = total_pages;
	z->bitmap = bitmap;

	z->fl_bitmap = 0;
	z->block_null.next = &z->block_null;
	z->block_null.prev = &z->block_null;
	z->block_null.size = 0;

	for (int i = 0; i < PMA_FL_INDEX_COUNT; i++) {
		z->sl_bitmap[i] = 0;
		for (int j = 0; j < PMA_SL_INDEX_COUNT; j++) {
			z->blocks[i][j] = &z->block_null;
		}
	}
}

// add a contiguous free region to the zone
static void pma_zone_add_region(pma_zone_t *z, uintptr_t phys_base, size_t length) {
	if (length < PMA_BLOCK_HEADER_SIZE) return;

	uintptr_t start = (phys_base + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
	uintptr_t end = (phys_base + length) & ~(uintptr_t)(PAGE_SIZE - 1);
	if (end <= start) return;

	// skip page 0 (null page trap)
	if (start == 0) {
		start += PAGE_SIZE;
		if (end <= start) return;
	}

	size_t region_bytes = end - start;

	// clear alloc bitmap for all pages in this region
	size_t start_pfn = start / PAGE_SIZE;
	size_t npages = region_bytes / PAGE_SIZE;

	for (size_t i = 0; i < npages; i++) {
		if (start_pfn + i < z->total_pages) {
			pma_page_set_free(z, start_pfn + i);
		}
	}

	pma_free_block_t *block = block_from_phys(start);
	block->size = region_bytes | 1;
	block->prev = NULL;
	block->next = NULL;

	tlsf_block_insert(z, block);
}

void pma_init(void) {
	uintptr_t ram_top = cmm_get_ram_top();
	size_t total_pages = ram_top / PAGE_SIZE;
	size_t bitmap_bytes = (total_pages + 7) / 8;
	size_t bitmap_reserved = ((bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

	// carve bitmap from first large USABLE region
	uint8_t *bitmap = NULL;
	uintptr_t bitmap_phys = 0;

	for (size_t i = 0; i < cmm_get_region_count(); i++) {
		const struct cmm_region *r = cmm_get_region_by_index(i);
		if (r->type == CMM_USABLE && r->length >= bitmap_reserved) {
			bitmap_phys = r->base;
			bitmap = (uint8_t *)(bitmap_phys + hhdm_offset);
			memset(bitmap, 0xFF, bitmap_bytes);
			break;
		}
	}

	if (!bitmap) panic("PMA", "failed to allocate zone bitmap");

	// both zones share the same bitmap
	pma_zone_init(&pma_normal, 0, total_pages, bitmap, "normal");
	pma_zone_init(&pma_low, 0, SMIN(total_pages, PMA_ZONE_32BIT), bitmap, "low");

	// walk CMM, add USABLE regions as free blocks
	for (size_t i = 0; i < cmm_get_region_count(); i++) {
		const struct cmm_region *r = cmm_get_region_by_index(i);

		// skip non-free memory types
		if (r->type != CMM_USABLE) continue;

		uintptr_t start = r->base;
		size_t len = r->length;

		// handle the region that contains the bitmap
		if (r->type == CMM_USABLE && start <= bitmap_phys && start + len > bitmap_phys) {
			// before bitmap
			if (bitmap_phys > start) {
				uintptr_t b_low_end = SMIN(bitmap_phys, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);

				if (b_low_end > start) {
					pma_zone_add_region(&pma_low, start, b_low_end - start);
				}

				uintptr_t b_norm_start = SMAX(start, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);

				if (bitmap_phys > b_norm_start){
					pma_zone_add_region(&pma_normal, b_norm_start, bitmap_phys - b_norm_start);
				}
			}

			for (size_t p = bitmap_phys / PAGE_SIZE; p < (bitmap_phys + bitmap_reserved) / PAGE_SIZE; p++) { if (p < total_pages) BITMAP_SET(bitmap, p); } // mark bitmap pages as used

			// after bitmap
			uintptr_t after = bitmap_phys + bitmap_reserved;
			if (after < start + len) {
				uintptr_t a_low_end = SMIN(start + len, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);
				if (a_low_end > after)
					pma_zone_add_region(&pma_low, after, a_low_end - after);

				uintptr_t a_norm_start = SMAX(after, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);
				if (start + len > a_norm_start)
					pma_zone_add_region(&pma_normal, a_norm_start, (start + len) - a_norm_start);
			}

			continue;
		}

		// The two zones manage disjoint page ranges:
		//   pma_low:   pages below PMA_ZONE_32BIT  (0 - 4 GB)
		//   pma_normal: pages at or above PMA_ZONE_32BIT
		uintptr_t low_start = start;
		uintptr_t low_end   = SMIN(start + len, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);
		if (low_end > low_start)
			pma_zone_add_region(&pma_low, low_start, low_end - low_start);

		uintptr_t normal_start = SMAX(start, (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE);
		uintptr_t normal_end   = start + len;
		if (normal_end > normal_start)
			pma_zone_add_region(&pma_normal, normal_start, normal_end - normal_start);
	}

}

// internal: allocate from a specific zone with required alignment
static uintptr_t pma_alloc_from_zone(pma_zone_t *z, size_t size, size_t align) {
	SPIN_LOCK(&z->lock);

	size = ALIGN(PAGE_SIZE, size);
	if (size < PMA_BLOCK_HEADER_SIZE) size = PMA_BLOCK_HEADER_SIZE;

	// extra room for worst-case alignment rounding
	size_t search_size = size;

	if (align > PAGE_SIZE) {
		search_size += align - PAGE_SIZE;
	}

	pma_free_block_t *block = tlsf_locate_free(z, search_size);

	if (!block) {
		SPIN_UNLOCK(&z->lock);
		return 0;
	}

	uintptr_t b_phys = block_phys(block);
	uintptr_t alloc_phys = (b_phys + align - 1) & ~(uintptr_t)(align - 1);

	// free the gap before the aligned address
	size_t head_gap = alloc_phys - b_phys;

	if (head_gap >= PMA_BLOCK_HEADER_SIZE) {
		pma_free_block_t *rest = block_split(block, head_gap);
		tlsf_block_insert(z, block);
		block = rest;
	} else if (head_gap > 0) { // gap too small to split, cannot use this block
		tlsf_block_insert(z, block);
		SPIN_UNLOCK(&z->lock);
		return 0;
	}

	size_t b_size = block_size(block);

	if (b_size > size + PMA_BLOCK_HEADER_SIZE) {
		pma_free_block_t *rest = block_split(block, size);
		tlsf_block_insert(z, rest);
	} else if (b_size < size) {
		tlsf_block_insert(z, block);
		SPIN_UNLOCK(&z->lock);
		return 0;
	}

	block_mark_used(block);

	size_t npages = size / PAGE_SIZE;

	for (size_t i = 0; i < npages; i++) {
		pma_page_set_used(z, (alloc_phys / PAGE_SIZE) + i);
	}

	SPIN_UNLOCK(&z->lock);
	return alloc_phys;
}

uintptr_t pma_alloc_pages(size_t count) {
	uintptr_t ret = pma_alloc_from_zone(&pma_normal, count * PAGE_SIZE, PAGE_SIZE);
	if (!ret) ret = pma_alloc_from_zone(&pma_low, count * PAGE_SIZE, PAGE_SIZE);
	return ret;
}

uintptr_t pma_alloc_pages_2mb(size_t count) {
	uintptr_t ret = pma_alloc_from_zone(&pma_normal, count * PAGE_SIZE_2MB, PAGE_SIZE_2MB);
	if (!ret) ret = pma_alloc_from_zone(&pma_low, count * PAGE_SIZE_2MB, PAGE_SIZE_2MB);
	return ret;
}

uintptr_t pma_alloc_pages_low(size_t count) {
	return pma_alloc_from_zone(&pma_low, count * PAGE_SIZE, PAGE_SIZE);
}

static void pma_zone_free_range(pma_zone_t *z, uintptr_t phys, size_t count) {
	if (!count) return;
	if (phys + count * PAGE_SIZE > z->total_pages * PAGE_SIZE) return;

	SPIN_LOCK(&z->lock);

	size_t start_pfn = phys / PAGE_SIZE;

	for (size_t i = 0; i < count; i++) {
		pma_page_set_free(z, start_pfn + i);
	}

	// coalesce backward
	size_t merge_start = start_pfn;
	if (start_pfn > 0 && pma_page_is_free(z, start_pfn - 1)) {
		size_t prev_start = pma_find_free_start(z, start_pfn - 1);
		pma_free_block_t *prev_block = block_from_phys(prev_start * PAGE_SIZE);
		tlsf_block_remove(z, prev_block);
		merge_start = prev_start;
	}

	// coalesce forward
	size_t merge_end = start_pfn + count;
	if (merge_end < z->total_pages && pma_page_is_free(z, merge_end)) {
		size_t next_end = pma_find_free_end(z, merge_end);
		pma_free_block_t *next_block = block_from_phys(merge_end * PAGE_SIZE);
		tlsf_block_remove(z, next_block);
		merge_end = next_end;
	}

	size_t merged_size = (merge_end - merge_start) * PAGE_SIZE;
	if (merged_size >= PMA_BLOCK_HEADER_SIZE) {
		pma_free_block_t *block = block_from_phys(merge_start * PAGE_SIZE);
		block->size = merged_size | 1;
		block->prev = NULL;
		block->next = NULL;
		tlsf_block_insert(z, block);
	}

	SPIN_UNLOCK(&z->lock);
}

void pma_free_pages(uintptr_t phys, size_t count) {
	if (!phys) return;

	uintptr_t low_limit = (uintptr_t)PMA_ZONE_32BIT * PAGE_SIZE;
	uintptr_t end = phys + count * PAGE_SIZE;

	uintptr_t low_start = phys;
	uintptr_t low_end = SMIN(end, low_limit);
	if (low_end > low_start)
		pma_zone_free_range(&pma_low, low_start, (low_end - low_start) / PAGE_SIZE);

	uintptr_t normal_start = SMAX(phys, low_limit);
	if (end > normal_start)
		pma_zone_free_range(&pma_normal, normal_start, (end - normal_start) / PAGE_SIZE);
}

void pma_free_page(uintptr_t phys) {
	pma_free_pages(phys, 1);
}

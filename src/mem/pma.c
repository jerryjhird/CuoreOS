#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include "mem.h"

#define PMA_PAGE_SIZE 4096

static uint8_t *pma_bitmap = NULL;
static size_t pma_total_pages = 0;
static size_t pma_bitmap_bytes = 0;
static uint64_t hhdm_offset = 0;

static inline void bit_set(size_t bit) {
    pma_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bit_clear(size_t bit) {
    pma_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bit_test(size_t bit) {
    return (pma_bitmap[bit / 8] >> (bit % 8)) & 1;
}

void pma_init(struct limine_memmap_response *map, struct limine_hhdm_response *hhdm) {
    hhdm_offset = hhdm->offset;
    uintptr_t max_phys = 0;

    for (size_t i = 0; i < map->entry_count; i++) {
        uintptr_t end = map->entries[i]->base + map->entries[i]->length;
        if (end > max_phys) max_phys = end;
    }

    pma_total_pages = max_phys / PMA_PAGE_SIZE;
    pma_bitmap_bytes = (pma_total_pages + 7) / 8;

    for (size_t i = 0; i < map->entry_count; i++) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= pma_bitmap_bytes) {
            pma_bitmap = (uint8_t *)(e->base + hhdm_offset);
            
            memset(pma_bitmap, 0xFF, pma_bitmap_bytes);

            e->base += pma_bitmap_bytes;
            e->length -= pma_bitmap_bytes;
            break;
        }
    }

    // mark usable regions as free
    for (size_t i = 0; i < map->entry_count; i++) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            for (uintptr_t addr = e->base; addr < e->base + e->length; addr += PMA_PAGE_SIZE) {
                bit_clear(addr / PMA_PAGE_SIZE);
            }
        }
    }
}

uintptr_t pma_alloc_pages(size_t count) {
    if (count == 0) return 0;
    size_t consecutive_found = 0;
    size_t start_bit = 0;

    for (size_t i = 0; i < pma_total_pages; i++) {
        if (!bit_test(i)) {
            if (consecutive_found == 0) start_bit = i;
            consecutive_found++;

            if (consecutive_found == count) {
                for (size_t j = 0; j < count; j++) bit_set(start_bit + j);
                return (uintptr_t)(start_bit * PMA_PAGE_SIZE);
            }
        } else {
            consecutive_found = 0;
        }
    }
    return 0; 
}

uintptr_t pma_alloc_page(void) { return pma_alloc_pages(1); }

void pma_free_pages(uintptr_t phys, size_t count) {
    size_t start_bit = phys / PMA_PAGE_SIZE;
    for (size_t i = 0; i < count; i++) bit_clear(start_bit + i);
}

void pma_free_page(uintptr_t phys) { pma_free_pages(phys, 1); }
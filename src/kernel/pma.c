#include "stdint.h"
#include "stdbool.h"
#include "arch/limine.h"
#include "memory.h"
#include "drivers/serial.h"

#define PMA_PAGE_SIZE 4096

static uint8_t *pma_bitmap;
static size_t pma_bitmap_bytes;

static size_t pma_total_pages;
static size_t pma_used_pages;

static inline void bit_set(size_t bit) {
    pma_bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static inline void bit_clear(size_t bit) {
    pma_bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static inline bool bit_test(size_t bit) {
    return (pma_bitmap[bit / 8] >> (bit % 8)) & 1u;
}

#define PMA_BITMAP_MAX_BYTES (1024 * 1024) // supports up to 32 GiB RAM
uint8_t pma_bitmap_storage[PMA_BITMAP_MAX_BYTES];

void pma_init(struct limine_memmap_response *mm) {
    uintptr_t max_addr = 0;

    // find highest physical address
    for (size_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        uintptr_t end = e->base + e->length;
        if (end > max_addr)
            max_addr = end;
    }

    pma_total_pages = 0;
    for (size_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;
    
        pma_total_pages += e->length / PMA_PAGE_SIZE;
    }
    pma_bitmap_bytes = (pma_total_pages + 7) / 8;

    if (pma_bitmap_bytes > PMA_BITMAP_MAX_BYTES) {
        // continuing would corrupt memory prob idk (TODO: Panic function to replace with)
        serial_write("pma_bitmap_bytes > PMA_BITMAP_MAX_BYTES", 39);
        while (1) { }
    }

    pma_bitmap = pma_bitmap_storage;

    // mark everything as used initially
    memset(pma_bitmap, 0xFF, pma_bitmap_bytes);
    pma_used_pages = pma_total_pages;

    // mark usable memory as free
    for (size_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];

        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uintptr_t start = e->base / PMA_PAGE_SIZE;
        uintptr_t count = e->length / PMA_PAGE_SIZE;

        for (uintptr_t f = 0; f < count; f++) {
            bit_clear(start + f);
            pma_used_pages--;
        }
    }
}

// allocation

uintptr_t pma_alloc_page(void) {
    for (size_t i = 0; i < pma_total_pages; i++) {
        if (!bit_test(i)) {
            bit_set(i);
            pma_used_pages++;
            return (uintptr_t)(i * PMA_PAGE_SIZE);
        }
    }
    return 0;
}

uintptr_t pma_alloc_pages(size_t count) {
    size_t run = 0;
    size_t start = 0;

    for (size_t i = 0; i < pma_total_pages; i++) {
        if (!bit_test(i)) {
            if (run == 0)
                start = i;

            run++;

            if (run == count) {
                for (size_t j = 0; j < count; j++)
                    bit_set(start + j);

                pma_used_pages += count;
                return (uintptr_t)(start * PMA_PAGE_SIZE);
            }
        } else {
            run = 0;
        }
    }
    return 0;
}


// free 

void pma_free_page(uintptr_t phys) {
    size_t page = phys / PMA_PAGE_SIZE;

    if (page >= pma_total_pages)
        return;

    if (bit_test(page)) {
        bit_clear(page);
        pma_used_pages--;
    }
}

void pma_free_pages(uintptr_t phys, size_t count) {
    size_t start = phys / PMA_PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        size_t page = start + i;

        if (page < pma_total_pages && bit_test(page)) {
            bit_clear(page);
            pma_used_pages--;
        }
    }
}

// info getters (seriously couldnt think of a better name)

size_t pma_page_size(void) {
    return PMA_PAGE_SIZE;
}

size_t pma_total(void) {
    return pma_total_pages;
}

size_t pma_used(void) {
    return pma_used_pages;
}

size_t pma_free(void) {
    return pma_total_pages - pma_used_pages;
}


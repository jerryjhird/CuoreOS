/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "stdint.h"
#include "stdbool.h"
#include "limine.h"
#include "memory.h"
#include "serial.h"
#include "liminereq.h"
#include "panic.h"
#include "PMA.h"

#define ALIGN_UP(x, a) (((x) + (uintptr_t)((a)-1)) & ~((uintptr_t)((a)-1)))

static uint8_t *pma_bitmap;
static size_t pma_bitmap_bytes;

size_t pma_total_pages;
size_t pma_used_pages;

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

void pma_init(void) {
    uintptr_t max_addr = 0;

    // find highest physical address
    for (size_t i = 0; i < memmap_req.response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_req.response->entries[i];
        uintptr_t end = e->base + e->length;
        if (end > max_addr)
            max_addr = end;
    }

    pma_total_pages = 0;
    for (size_t i = 0; i < memmap_req.response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_req.response->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;
    
        pma_total_pages += e->length / PMA_PAGE_SIZE;
    }
    pma_bitmap_bytes = (pma_total_pages + 7) / 8;

    if (pma_bitmap_bytes > PMA_BITMAP_MAX_BYTES) {
        nl_serial_write("kernel panic!\npma_bitmap_bytes > PMA_BITMAP_MAX_BYTES\n");
        panic();
    }

    pma_bitmap = pma_bitmap_storage;

    memset(pma_bitmap, 0xFF, pma_bitmap_bytes);
    pma_used_pages = pma_total_pages;

    // mark usable memory as free
    for (size_t i = 0; i < memmap_req.response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_req.response->entries[i];

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

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }

    return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }

    return 0;
}

void ptrhex(char *buf, void *ptr) {
    uintptr_t val = (uintptr_t)ptr;
    const char hex[] = "0123456789ABCDEF";
    for (int i = (sizeof(void*)*2)-1; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[sizeof(void*)*2] = 0;
}

uint8_t* heap_ptr = (uint8_t*)0x100000;  // placeholder (these are set using heapinit and no heap allocation should be done beforehand)
uint8_t* heap_end = (uint8_t*)0x200000; // placeholder

bool heap_can_alloc(size_t size) {
    uintptr_t ptr = (uintptr_t)heap_ptr;
    ptr = ALIGN_UP(ptr, 8);

    size_t total_size = size + sizeof(size_t);
    return (ptr + total_size <= (uintptr_t)heap_end);
}

void* malloc(size_t size) {
    if (!size) return NULL;

    // space for storing size before the block
    size_t total_size = size + sizeof(size_t);

    uintptr_t ptr = (uintptr_t)heap_ptr;
    ptr = ALIGN_UP(ptr, 8); // 8-byte alignment

    if (ptr + total_size > (uintptr_t)heap_end) return NULL;

    // store size before block
    *((size_t*)ptr) = size;

    heap_ptr = (uint8_t*)(ptr + total_size);
    return (void*)(ptr + sizeof(size_t)); // return pointer after size
}

void sfree(void* ptr, size_t size) {
    uintptr_t block = (uintptr_t)ptr;
    if (block + size == (uintptr_t)heap_ptr) {
        heap_ptr = (uint8_t*)block; // rewind heap pointer
    }
}

void free(void* user_ptr) {
    if (!user_ptr) return;

    uintptr_t block_ptr = (uintptr_t)user_ptr;
    size_t size = *((size_t*)(block_ptr - sizeof(size_t)));
    uintptr_t block_start = block_ptr - sizeof(size_t);

    sfree((void*)block_start, size + sizeof(size_t));
}

void* zalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;        
    memset(ptr, 0, size);
    return ptr;
}

void heapinit(uint8_t* start, uint8_t* end) {
    heap_ptr = start;
    heap_end = end;
}
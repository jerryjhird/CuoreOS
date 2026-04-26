#pragma once

#include <stddef.h>
#include <stdint.h>

#define PMA_ZONE_32BIT (0x100000000ULL / PAGE_SIZE)

void pma_init(void);

uintptr_t pma_alloc_pages(size_t count); // allocate anywhere in physical memory
uintptr_t pma_alloc_pages_low(size_t count); // allocate below 4gb

void pma_free_pages(uintptr_t phys, size_t count);
void pma_free_page(uintptr_t phys);

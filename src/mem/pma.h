#pragma once

#include <stddef.h>
#include <stdint.h>

void pma_init(struct limine_memmap_response *map, struct limine_hhdm_response *hhdm);
uintptr_t pma_alloc_pages(size_t count);
uintptr_t pma_alloc_page(void);
void pma_free_pages(uintptr_t phys, size_t count);
void pma_free_page(uintptr_t phys);
#pragma once

#include <stddef.h>
#include <stdint.h>

void pma_init(void);
uintptr_t pma_alloc_pages(size_t count);
uintptr_t pma_alloc_page(void);
void pma_free_pages(uintptr_t phys, size_t count);
void pma_free_page(uintptr_t phys);

// stats
size_t pma_get_free_pages(void);
size_t pma_get_used_pages(void);
size_t pma_get_total_pages(void);

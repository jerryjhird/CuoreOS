/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef PMA_H
#define PMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define PMA_PAGE_SIZE 4096
extern size_t pma_total_pages;
extern size_t pma_used_pages;

void pma_init(void);

// pma allocation
uintptr_t pma_alloc_page(void);
uintptr_t pma_alloc_pages(size_t count);

// pma free
void pma_free_page(uintptr_t phys);
void pma_free_pages(uintptr_t phys, size_t count);

#ifdef __cplusplus
}
#endif

#endif // PMA_H
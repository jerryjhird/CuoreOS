/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef PAGING_H
#define PAGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stddef.h"

#define PAGE_SIZE 4096
#define PML4_ENTRIES 512
#define PDPT_ENTRIES 512
#define PD_ENTRIES 512
#define PT_ENTRIES 512

// page flags
#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_RW        (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
#define PAGE_PWT       (1ULL << 3)
#define PAGE_PCD       (1ULL << 4)
#define PAGE_ACCESSED  (1ULL << 5)
#define PAGE_DIRTY     (1ULL << 6)
#define PAGE_PSE       (1ULL << 7)
#define PAGE_NX        (1ULL << 63)

void map_page(uint64_t* pml4, uintptr_t vaddr, uintptr_t paddr, uint64_t flags);
void map_pages(size_t count, uint64_t* pml4, uintptr_t vaddr, uintptr_t paddr, uint64_t flags);

#ifdef __cplusplus
}
#endif

#endif // PAGING_H
/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <string.h>
#include "limine.h"
#include "klimine.h"
#include "PMA.h"
#include "paging.h"

// assumes pml4 points to top level page table 4kb-aligned
static inline uint64_t* get_or_alloc_table(uint64_t* entry) {
    if (!(*entry & 1)) { // not present
        uint64_t phys = pma_alloc_page();
        uint64_t* table = (uint64_t*)(phys + hhdm_req.response->offset);
        memset(table, 0, PAGE_SIZE);
        *entry = phys | PAGE_PRESENT | PAGE_RW | PAGE_USER; // new table
        return table;
    } else {
        // ensure USER
        *entry |= PAGE_USER;
        return (uint64_t*)(*entry & ~0xFFFULL + hhdm_req.response->offset);
    }
}

void map_page(uint64_t* pml4, uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    size_t pml4_index = (vaddr >> 39) & 0x1FF;
    size_t pdpt_index = (vaddr >> 30) & 0x1FF;
    size_t pd_index   = (vaddr >> 21) & 0x1FF;
    size_t pt_index   = (vaddr >> 12) & 0x1FF;

    uint64_t* pdpt = get_or_alloc_table(&pml4[pml4_index]);
    uint64_t* pd   = get_or_alloc_table(&pdpt[pdpt_index]);
    uint64_t* pt   = get_or_alloc_table(&pd[pd_index]);

    pt[pt_index] = paddr | flags | PAGE_PRESENT;
}

void map_pages(size_t count, uint64_t* pml4, uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    for (size_t i = 0; i < count; i++) {
        map_page(pml4, vaddr + i * PAGE_SIZE, paddr + i * PAGE_SIZE, flags);
    }
}
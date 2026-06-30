#pragma once

#include <stdint.h>
#include <stddef.h>

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_HUGE_PAGE (1ULL << 7)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_PAT (1ULL << 12)
#define PTE_NX (1ULL << 63)
#define PTE_ADDR_MASK 0x000FFFFFFFFFFFF000ULL

// custom PTE attributes
#define PTE_STATE_NOINFO 0
#define PTE_STATE_HEAP_BLOCK 1
#define PTE_STATE_HEAP_POOL 2
#define PTE_STATE_RESERVED0 3
#define PTE_STATE_RESERVED1 4
#define PTE_STATE_RESERVED2 5
#define PTE_STATE_RESERVED3 6
#define PTE_STATE_RESERVED4 7

#define PTE_TYPE_NOINFO (0ULL << 9)
#define PTE_TYPE_HEAP_BLOCK (1ULL << 9)
#define PTE_TYPE_HEAP_POOL (2ULL << 9)
#define PTE_TYPE_RESERVED0 (3ULL << 9)
#define PTE_TYPE_RESERVED1 (4ULL << 9)
#define PTE_TYPE_RESERVED2 (5ULL << 9)
#define PTE_TYPE_RESERVED3 (6ULL << 9)
#define PTE_TYPE_RESERVED4 (7ULL << 9)

void paging_init(void);

// 4kb page mappings
void paging_map_page(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_map_page_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_unmap_page(uint64_t* pml4, uintptr_t virt);
void paging_unmap_page_noflush(uint64_t* pml4, uintptr_t virt);

// 2mb huge page mappings
void paging_map_2mb(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_map_2mb_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_unmap_2mb(uint64_t* pml4, uintptr_t virt);
void paging_unmap_2mb_noflush(uint64_t* pml4, uintptr_t virt);

// 1gb huge page mappings
void paging_map_1gb(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_map_1gb_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void paging_unmap_1gb(uint64_t* pml4, uintptr_t virt);
void paging_unmap_1gb_noflush(uint64_t* pml4, uintptr_t virt);

uint64_t* paging_get_pte(uint64_t* pml4, uintptr_t virt, int allocate);
uint64_t paging_get_pml4(void);
size_t paging_get_level(void);
void paging_set_pml4(uint64_t pml4_phys);
uint64_t paging_read_cr4(void);
uintptr_t paging_get_phys(uint64_t* pml4, uintptr_t vaddr);

void paging_flush_tlb_page(uintptr_t virt);
void paging_flush_tlb_all(void);

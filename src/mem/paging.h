#pragma once

#include <stdint.h>

#define PTE_PRESENT (1ULL << 0) // 0x01
#define PTE_WRITABLE (1ULL << 1) // 0x02
#define PTE_USER (1ULL << 2) // 0x04
#define PTE_WRITE_THROUGH (1ULL << 3) // 0x08 - PWT
#define PTE_CACHE_DISABLE (1ULL << 4) // 0x10 - PCD
#define PTE_ACCESSED (1ULL << 5) // 0x20
#define PTE_DIRTY (1ULL << 6) // 0x40
#define PTE_HUGE_PAGE (1ULL << 7) // 0x80
#define PTE_GLOBAL (1ULL << 8) // 0x100
#define PTE_NX (1ULL << 63)// No Execute

// custom PTE attributes
#define PTE_STATE_NOINFO 0	  // 0 (000)
#define PTE_STATE_HEAP_BLOCK 1  // 1 (001)
#define PTE_STATE_HEAP_POOL 2   // 2 (010)
#define PTE_STATE_DRIVER 3	  // 3 (011)
#define PTE_STATE_RESERVED0 4   // 4 (100)
#define PTE_STATE_RESERVED1 5   // 5 (101)
#define PTE_STATE_RESERVED2 6   // 6 (110)
#define PTE_STATE_RESERVED3 7   // 7 (111)

#define PTE_TYPE_NOINFO (0ULL << 9)	 // Bit 11=0 | Bit 10=0 | Bit 9=0
#define PTE_TYPE_HEAP_BLOCK (1ULL << 9) // Bit 11=0 | Bit 10=0 | Bit 9=1
#define PTE_TYPE_HEAP_POOL (2ULL << 9)  // Bit 11=0 | Bit 10=1 | Bit 9=0
#define PTE_TYPE_DRIVER (3ULL << 9)	 // Bit 11=0 | Bit 10=1 | Bit 9=1
#define PTE_TYPE_RESERVED0 (4ULL << 9)  // Bit 11=1 | Bit 10=0 | Bit 9=0
#define PTE_TYPE_RESERVED1 (5ULL << 9)  // Bit 11=1 | Bit 10=0 | Bit 9=1
#define PTE_TYPE_RESERVED2 (6ULL << 9)  // Bit 11=1 | Bit 10=1 | Bit 9=0
#define PTE_TYPE_RESERVED3 (7ULL << 9)  // Bit 11=1 | Bit 10=1 | Bit 9=1

void vmm_map_page(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
uint64_t* vmm_get_pte(uint64_t* pml4, uintptr_t virt, int allocate);
uint64_t vmm_get_pml4(void);

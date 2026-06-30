#include "paging.h"

#include "mem.h"
#include "pma.h"
#include "panic.h"

static size_t paging_level = 4;

void paging_init(void) {
	uint64_t cr4 = paging_read_cr4();
	paging_level = (cr4 & (1ULL << 12)) ? 5 : 4;
}

static uint64_t* walk_to_level(uint64_t* top_table, uintptr_t virt, int target_level, int allocate) {
	uint64_t* table = top_table;
	int top = (int)paging_level - 1;

	for (int level = top; level > target_level; level--) {
		uint64_t index = (virt >> (12 + level * 9)) & 0x1FF;
		uint64_t entry = table[index];

		if (!(entry & PTE_PRESENT)) {
			if (!allocate) return NULL;
			uintptr_t new_table_phys = pma_alloc_pages(1);
			if (!new_table_phys) return NULL;
			uint64_t* new_table_virt = (uint64_t*)(new_table_phys + hhdm_offset);
			memset(new_table_virt, 0, PAGE_SIZE);
			table[index] = new_table_phys | (PTE_PRESENT | PTE_WRITABLE);
			table = new_table_virt;
		} else if (entry & PTE_HUGE_PAGE) {
			if (!allocate) return NULL;
			// split huge page: allocate next-level table and populate sub-entries
			uintptr_t new_table_phys = pma_alloc_pages(1);
			if (!new_table_phys) return NULL;
			uint64_t* new_table_virt = (uint64_t*)(new_table_phys + hhdm_offset);
			uintptr_t base_phys = entry & ~((1ULL << (12 + level * 9)) - 1);
			uint64_t sub_flags = (entry & 0xFFF) | (entry & PTE_NX);
			sub_flags &= ~PTE_HUGE_PAGE;
			sub_flags &= ~PTE_GLOBAL;
			size_t sub_page_size = 1ULL << (12 + (level - 1) * 9);

			for (size_t i = 0; i < 512; i++) {
				new_table_virt[i] = (base_phys + i * sub_page_size) | sub_flags;
				if (level > 1)
					new_table_virt[i] |= PTE_HUGE_PAGE;
			}

			uint64_t table_flags = entry & (PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX);
			table[index] = new_table_phys | table_flags;
			table = new_table_virt;
		} else {
			table = (uint64_t*)((entry & PTE_ADDR_MASK) + hhdm_offset);
		}
	}

	return table;
}

static uint64_t* get_page_entry(uint64_t* top_table, uintptr_t vaddr) {
	uint64_t* table = top_table;
	int top = (int)paging_level - 1;

	for (int level = top; level >= 0; level--) {
		uint64_t index = (vaddr >> (12 + level * 9)) & 0x1FF;
		uint64_t* entry_ptr = &table[index];
		uint64_t entry = *entry_ptr;

		if (!(entry & PTE_PRESENT)) return NULL;

		// If it's a huge page or we reached the 4KB level (level 0)
		if ((entry & PTE_HUGE_PAGE) || level == 0) {
			return entry_ptr;
		}

		// Keep walking
		table = (uint64_t*)((entry & PTE_ADDR_MASK) + hhdm_offset);
	}
	return NULL;
}

uint64_t* paging_get_pte(uint64_t* pml4, uintptr_t virt, int allocate) {
	uint64_t* pt = walk_to_level(pml4, virt, 0, allocate);
	if (!pt) return NULL;
	return &pt[(virt >> 12) & 0x1FF];
}

// 4kb page mappings

void paging_map_page_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	uint64_t* pte = paging_get_pte(pml4, virt, 1);
	if (!pte) return;
	*pte = (phys & ~0xFFFULL) | flags;
}

void paging_map_page(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	paging_map_page_noflush(pml4, virt, phys, flags);
	paging_flush_tlb_page(virt);
}

void paging_unmap_page_noflush(uint64_t* pml4, uintptr_t virt) {
	uint64_t* pte = paging_get_pte(pml4, virt, 0);
	if (pte && (*pte & PTE_PRESENT)) {
		*pte = 0;
	}
}

void paging_unmap_page(uint64_t* pml4, uintptr_t virt) {
	paging_unmap_page_noflush(pml4, virt);
	paging_flush_tlb_page(virt);
}

// 2mb huge page mappings

void paging_map_2mb_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	uint64_t* pd = walk_to_level(pml4, virt, 1, 1);
	if (!pd) return;
	pd[(virt >> 21) & 0x1FF] = (phys & ~(uintptr_t)(PAGE_SIZE_2MB - 1)) | flags | PTE_HUGE_PAGE;
}

void paging_map_2mb(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	paging_map_2mb_noflush(pml4, virt, phys, flags);
	paging_flush_tlb_page(virt);
}

void paging_unmap_2mb_noflush(uint64_t* pml4, uintptr_t virt) {
	uint64_t* pd = walk_to_level(pml4, virt, 1, 0);
	if (!pd) return;
	size_t index = (virt >> 21) & 0x1FF;
	if (pd[index] & PTE_PRESENT) pd[index] = 0;
}

void paging_unmap_2mb(uint64_t* pml4, uintptr_t virt) {
	paging_unmap_2mb_noflush(pml4, virt);
	paging_flush_tlb_page(virt);
}

// 1gb huge page mappings

void paging_map_1gb_noflush(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	uint64_t* pdpt = walk_to_level(pml4, virt, 2, 1);
	if (!pdpt) return;
	pdpt[(virt >> 30) & 0x1FF] = (phys & ~(uintptr_t)(PAGE_SIZE_1GB - 1)) | flags | PTE_HUGE_PAGE;
}

void paging_map_1gb(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	paging_map_1gb_noflush(pml4, virt, phys, flags);
	paging_flush_tlb_page(virt);
}

void paging_unmap_1gb_noflush(uint64_t* pml4, uintptr_t virt) {
	uint64_t* pdpt = walk_to_level(pml4, virt, 2, 0);
	if (!pdpt) return;
	size_t index = (virt >> 30) & 0x1FF;
	if (pdpt[index] & PTE_PRESENT) pdpt[index] = 0;
}

void paging_unmap_1gb(uint64_t* pml4, uintptr_t virt) {
	paging_unmap_1gb_noflush(pml4, virt);
	paging_flush_tlb_page(virt);
}

uintptr_t paging_get_phys(uint64_t* top_table, uintptr_t vaddr) {
	uint64_t* table = top_table;
	int top = (int)paging_level - 1;

	for (int level = top; level > 0; level--) {
		uint64_t index = (vaddr >> (12 + level * 9)) & 0x1FF;
		uint64_t entry = table[index];
		if (!(entry & PTE_PRESENT)) return 0;
		if (entry & PTE_HUGE_PAGE) {
			uint64_t page_offset_mask = (1ULL << (12 + level * 9)) - 1;
			return (entry & ~page_offset_mask) + (vaddr & page_offset_mask);
		}
		table = (uint64_t*)((entry & PTE_ADDR_MASK) + hhdm_offset);
	}

	// level 0 (PT)
	uint64_t entry = table[(vaddr >> 12) & 0x1FF];
	if (!(entry & PTE_PRESENT)) return 0;
	return (entry & PTE_ADDR_MASK) + (vaddr & 0xFFF);
}

size_t paging_get_level(void) {
	return paging_level;
}

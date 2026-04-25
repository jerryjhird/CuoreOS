#include "paging.h"

#include "mem.h"
#include "pma.h"

uint64_t* vmm_get_pte(uint64_t* pml4, uintptr_t virt, int allocate) {
	uint64_t* table = pml4;

	for (int level = 3; level > 0; level--) {
		uint64_t index = (virt >> (12 + (level * 9))) & 0x1FF;
		uint64_t entry = table[index];

		if (!(entry & 1)) {
			if (!allocate) return NULL;
			uintptr_t new_table_phys = pma_alloc_page();
			uint64_t* new_table_virt = (uint64_t*)(new_table_phys + hhdm_offset);

			memset(new_table_virt, 0, 4096);

			table[index] = new_table_phys | 0x03;
			table = new_table_virt;
		} else {
			table = (uint64_t*)((entry & ~0xFFF) + hhdm_offset);
		}
	}

	uint64_t index = (virt >> 12) & 0x1FF;
	return &table[index];
}

void vmm_map_page(uint64_t* pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
	uint64_t* pte = vmm_get_pte(pml4, virt, 1);
	if (!pte) return;

	*pte = (phys & ~0xFFF) | flags;

	__asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uintptr_t vmm_get_phys(uint64_t* pml4, uintptr_t vaddr) {
	// PML4
	uint64_t pml4e = pml4[(vaddr >> 39) & 0x1FF];
	if (!(pml4e & 1)) return 0;

	// PDPT
	uint64_t* pdpt = (uint64_t*)((pml4e & ~0xFFF) + hhdm_offset);
	uint64_t pdpte = pdpt[(vaddr >> 30) & 0x1FF];
	if (!(pdpte & 1)) return 0;

	// 1GB Huge Page
	if (pdpte & (1 << 7)) return (pdpte & ~0x3FFFFFFF) + (vaddr & 0x3FFFFFFF);

	// PD
	uint64_t* pd = (uint64_t*)((pdpte & ~0xFFF) + hhdm_offset);
	uint64_t pde = pd[(vaddr >> 21) & 0x1FF];
	if (!(pde & 1)) return 0;

	// 2MB Huge Page
	if (pde & (1 << 7)) return (pde & ~0x1FFFFF) + (vaddr & 0x1FFFFF);

	// PT
	uint64_t* pt = (uint64_t*)((pde & ~0xFFF) + hhdm_offset);
	uint64_t pte = pt[(vaddr >> 12) & 0x1FF];
	if (!(pte & 1)) return 0;

	return (pte & ~0xFFF) + (vaddr & 0xFFF);
}

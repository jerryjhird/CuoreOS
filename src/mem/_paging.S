.intel_syntax noprefix
.global paging_get_pml4
.global paging_flush_tlb_page
.global paging_flush_tlb_all
.global paging_set_pml4

paging_get_pml4:
	mov rax, cr3
	ret

# void paging_set_pml4(uint64_t pml4_phys);
paging_set_pml4:
	mov cr3, rdi
	ret

# void paging_flush_tlb_page(uintptr_t virt);
paging_flush_tlb_page:
	invlpg [rdi]
	ret

# void paging_flush_tlb_all(void);
paging_flush_tlb_all:
	mov rax, cr3
	mov cr3, rax
	ret

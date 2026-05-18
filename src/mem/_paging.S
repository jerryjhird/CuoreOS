.intel_syntax noprefix
.global vmm_get_pml4
.global vmm_flush_tlb_page
.global vmm_flush_tlb_all
.global vmm_set_pml4

vmm_get_pml4:
	mov rax, cr3
	ret

# void vmm_set_pml4(uint64_t pml4_phys);
vmm_set_pml4:
	mov cr3, rdi
	ret

# void vmm_flush_tlb_page(uintptr_t virt);
vmm_flush_tlb_page:
	invlpg [rdi]
	ret

# void vmm_flush_tlb_all(void);
vmm_flush_tlb_all:
	mov rax, cr3
	mov cr3, rax
	ret

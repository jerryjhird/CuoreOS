.intel_syntax noprefix
.global paging_get_pml4
.global paging_flush_tlb_page
.global paging_flush_tlb_all
.global paging_set_pml4
.global paging_read_cr4

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
	push rcx
	push rax

	# disable global pages to flush global TLB entries
	mov rax, cr4
	mov rcx, rax
	and rax, ~0x80 # clear CR4.PGE
	mov cr4, rax
	mov cr4, rcx # restore CR4.PGE
	pop rax
	pop rcx
	ret

# uint64_t paging_read_cr4(void);
paging_read_cr4:
	mov rax, cr4
	ret

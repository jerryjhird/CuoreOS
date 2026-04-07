.intel_syntax noprefix
.global vmm_get_pml4

vmm_get_pml4:
    mov rax, cr3
    ret

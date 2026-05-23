.intel_syntax noprefix

.section .text
.global AP_kentry
.extern AP_kstartc
.extern kernel_pml4_phys

AP_kentry:
	mov rax, [rip + kernel_pml4_phys]
	mov cr3, rax

	mov rsp, [rdi + 24]
	xor rbp, rbp
	and rsp, -16

	jmp AP_kstartc

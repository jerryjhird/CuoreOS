.intel_syntax noprefix

.section .text
.global AP_kentry
.extern AP_kstartc
.extern kernel_pml4_phys
.extern kernel_cr4

AP_kentry:
	mov rax, [rip + kernel_pml4_phys]
	mov cr3, rax

	// copy cr4 from bsp (preserves la57, osfxsr, osxmmexcpt, etc)
	mov rax, [rip + kernel_cr4]
	mov cr4, rax

	// enable sse bits in cr4 (in case bsp didn't have them)
	mov rax, cr4
	bts rax, 9 // set osfxsr
	bts rax, 10	// set osxmmexcpt
	mov cr4, rax
	ldmxcsr [rip + mxcsr_default]

	mov rsp, [rdi + 24]
	xor rbp, rbp
	and rsp, -16

	jmp AP_kstartc

.section .rodata
.align 4
mxcsr_default: .int 0x1FC0

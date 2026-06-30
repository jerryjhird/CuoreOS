.section .rodata
retpanic_title: .asciz "UNEXPECTED RETURN"
retpanic_msg:   .asciz "_kstartc returned"
.align 4
mxcsr_default: .int 0x1FC0

.section .text
.global _kstart
_kstart:
	cli

	// enable sse
	movq %cr0, %rax
	btr $2, %rax // clear em (bit 2), disable fpu emulation
	bts $1, %rax // set mp (bit 1), monitor co-processor
	movq %rax, %cr0
	movq %cr4, %rax
	bts $9, %rax // set osfxsr (bit 9), fxsave/fxrstor
	bts $10, %rax // set osxmmexcpt (bit 10), simd exceptions
	movq %rax, %cr4
	ldmxcsr mxcsr_default(%rip)

	movabs $_stack_end, %rsp
	subq $8, %rsp
	movq $0, %rbp

	call _kstartc

	// if _kstartc returns, panic
	movabs $retpanic_title, %rdi
	movabs $retpanic_msg, %rsi
	call panic

1:  hlt
	jmp 1b

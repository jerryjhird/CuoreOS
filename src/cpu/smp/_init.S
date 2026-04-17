.intel_syntax noprefix

.section .text
.global AP_kentry
.extern AP_kstartc

AP_kentry:
    mov rsp, [rdi + 24]
    xor rbp, rbp

    and rsp, -16

    jmp AP_kstartc

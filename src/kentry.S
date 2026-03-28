.global _kstart

.section .text
_kstart:
    subq $8, %rsp

    cli

    /* setup the stack */
    movabs $_stack_end, %rsp
    movq $0, %rbp

    call _kstartc

    /* halt */
1:  hlt
    jmp 1b
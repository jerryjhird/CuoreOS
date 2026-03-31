.global _kstart

.section .text
_kstart:
    cli
    movabs $_stack_end, %rsp

    subq $8, %rsp

    movq $0, %rbp
    call _kstartc

    call _kstartc

    /* halt */
1:  hlt
    jmp 1b
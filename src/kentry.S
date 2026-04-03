.section .rodata
retpanic_title: .asciz "UNEXPECTED RETURN"
retpanic_msg:   .asciz "_kstartc returned"

.section .text
.global _kstart
_kstart:
    cli
    movabs $_stack_end, %rsp
    subq $8, %rsp
    movq $0, %rbp

    call _kstartc

    /* if _kstartc returns, panic*/
    movabs $retpanic_title, %rdi
    movabs $retpanic_msg, %rsi
    call panic

1:  hlt
    jmp 1b
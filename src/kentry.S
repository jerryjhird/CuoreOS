.global _kstart

.section .text
_kstart:
    subq $8, %rsp

    /* setup the stack */
    movabs $_stack_end, %rsp
    movq $0, %rbp

    /* init serial port (UART) */
    call uart16550_init

    /* init GDT and IDT */
    call gdt_init
    call idt_init

    call _kstartc

    /* halt */
1:  hlt
    jmp 1b
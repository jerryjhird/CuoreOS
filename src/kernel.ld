OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X */
    data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    /* kernel virtual base address */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    } :text

    .rodata : {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(0x1000);

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(0x1000);
    } :data

    .stack : {
        . = ALIGN(16);
        _stack_start = .;
        . += 16K;
        _stack_end = .;
    } :data
}

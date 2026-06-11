OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_kstart)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R + X */
    rodata PT_LOAD FLAGS(4);   /* R */
    data   PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    /* kernel virtual base address */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    } :text

    .csig : {
        _sig_start = .;
        KEEP(*(.csig))
        . = _sig_start + 8;
        _sig_end = .;
    } :text

    ASSERT(_sig_end - _sig_start == 8, ".csig section must be exactly 8 bytes!")

    . = ALIGN(0x1000);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);

    .limine_requests : {
        _limine_requests_start = .;
        KEEP(*(.limine_requests .limine_requests.*))
        _limine_requests_end = .;
    } :data

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(0x1000);

    .stack : {
        . = ALIGN(16);
        _stack_start = .;
        . += 16K;
        _stack_end = .;
    } :data
}

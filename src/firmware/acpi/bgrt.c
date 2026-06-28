#include "bgrt.h"

#include "logbuf.h"
#include <stdint.h>
#include "mem/mem.h"

uintptr_t bgrt_vaddr = 0;

void bgrt_init(struct acpi_sdt_header* acpi_tab) {
    struct bgrt_table* bgrt = (struct bgrt_table*)acpi_tab;

    if (!(bgrt->status & 0x01) || (bgrt->image_type != BGRT_IMAGE_TYPE_BMP)) {
        return;
    }

    bgrt_vaddr = (uintptr_t)bgrt->image_address + hhdm_offset;
}

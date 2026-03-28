#include "madt.h"
#include "acpi/acpi.h"
#include "kstate.h"
#include <stddef.h>

static uintptr_t lapic_phys = 0;
static uintptr_t ioapic_phys = 0;

void madt_init(void) {
    struct madt* table = (struct madt*)acpi_find_sdt("APIC");
    if (!table) {
        panic("MADT", "Could not find MADT table!");
    }

    lapic_phys = table->lapic_addr;

    uint8_t* ptr = table->entries;
    uint8_t* end = (uint8_t*)table + table->header.length;

    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t len  = ptr[1];

        switch (type) {
            case MADT_TYPE_LAPIC:
                break;

            case MADT_TYPE_IOAPIC: {
                ioapic_phys = (uintptr_t)(*(uint32_t*)(ptr + 4));
                break;
            }

            case MADT_TYPE_ISO: {
                break;
            }
        }
        ptr += len;
    }
}

uintptr_t madt_get_lapic_base()   { return lapic_phys; }
uintptr_t madt_get_ioapic_base() { return ioapic_phys; }
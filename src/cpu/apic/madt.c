#include "madt.h"
#include "firmware/acpi/acpi.h"
#include "ioapic.h"
#include <stddef.h>
#include "mem/mem.h"
#include "panic.h"

static uintptr_t lapic_phys = 0;
static uintptr_t ioapic_phys = 0;

static madt_iso_t isos[16];
static int iso_count = 0;

void madt_init(void) {
	struct madt* table = (struct madt*)acpi_find_sdt("APIC");
	if (!table) {
		panic("MADT", "MADT table is missing or corrupted.");
	}

	lapic_phys = table->lapic_addr;

	uint8_t* ptr = table->entries;
	uint8_t* end = (uint8_t*)table + table->header.length;

	while (ptr < end) {
		uint8_t type = ptr[0];
		uint8_t len  = ptr[1];

		if (len == 0) break;

		switch (type) {
			case MADT_TYPE_LAPIC:
				break;

			case MADT_TYPE_IOAPIC: {
				uint32_t ioapic_addr_val;
				memcpy(&ioapic_addr_val, ptr + 4, sizeof(uint32_t));
				ioapic_phys = (uintptr_t)ioapic_addr_val;
				break;
			}

			case MADT_TYPE_ISO: {
				if (iso_count < 16) {
					isos[iso_count].bus_source = ptr[3];
					memcpy(&isos[iso_count].gsi, ptr + 4, sizeof(uint32_t));
					memcpy(&isos[iso_count].flags, ptr + 8, sizeof(uint16_t));
					iso_count++;
				}
				break;
			}
		}
		ptr += len;
	}
}

void madt_query_irq(uint8_t irq, uint8_t *out_pin, uint32_t *out_flags) {
	*out_pin = irq;
	*out_flags = IOAPIC_TRIGGER_EDGE | IOAPIC_POLARITY_HIGH;

	for (int i = 0; i < iso_count; i++) {
		if (isos[i].bus_source == irq) {
			*out_pin = (uint8_t)isos[i].gsi;

			if ((isos[i].flags & 3) == 3) *out_flags |= IOAPIC_POLARITY_LOW;
			// Trigger: 1=Edge, 3=Level
			if (((isos[i].flags >> 2) & 3) == 3) *out_flags |= IOAPIC_TRIGGER_LEVEL;

			return;
		}
	}
}

uintptr_t madt_get_lapic_base(void) { return lapic_phys; }
uintptr_t madt_get_ioapic_base(void) { return ioapic_phys; }

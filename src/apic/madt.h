#pragma once

#include <stdint.h>
#include "acpi/acpi.h"

struct madt {
	struct acpi_sdt_header header;
	uint32_t lapic_addr;
	uint32_t flags;
	uint8_t entries[];
} __attribute__((packed));

typedef struct {
	uint8_t bus_source;
	uint32_t gsi;
	uint16_t flags;
} madt_iso_t;

#define MADT_TYPE_LAPIC 0
#define MADT_TYPE_IOAPIC 1
#define MADT_TYPE_ISO 2
#define MADT_TYPE_NMI 4

void madt_init(void);

void madt_query_irq(uint8_t irq, uint8_t *out_pin, uint32_t *out_flags);

uintptr_t madt_get_lapic_base(void);
uintptr_t madt_get_ioapic_base(void);
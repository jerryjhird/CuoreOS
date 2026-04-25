#pragma once

#include <stdint.h>
#include "acpi.h"

struct mcfg_entry {
	uint64_t base_address;
	uint16_t segment_group;
	uint8_t  start_bus;
	uint8_t  end_bus;
	uint32_t reserved;
} __attribute__((packed));

struct mcfg_table {
	struct acpi_sdt_header header;
	uint64_t reserved;
	struct mcfg_entry entries[];
} __attribute__((packed));

void mcfg_init(void);
void* mcfg_get_device_addr(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func);

#pragma once

#include <stdint.h>
#include "firmware/acpi/acpi.h"

#define HPET_REG_CAPS 0x00
#define HPET_REG_CONFIG 0x10
#define HPET_REG_MAIN_COUNTER 0xF0

struct hpet_table {
	struct acpi_sdt_header header;
	uint32_t event_timer_block_id;
	struct {
		uint8_t address_space_id;
		uint8_t register_bit_width;
		uint8_t register_bit_offset;
		uint8_t reserved;
		uint64_t address;
	} __attribute__((packed)) address;
	uint8_t hpet_number;
	uint16_t minimum_tick;
	uint8_t page_protection;
} __attribute__((packed));

void hpet_init(struct acpi_sdt_header* header);
uint64_t hpet_get_nanos(void);
uint64_t hpet_get_ms(void);
uint32_t hpet_femto_per_tick(void);
uint64_t hpet_get_ticks(void);

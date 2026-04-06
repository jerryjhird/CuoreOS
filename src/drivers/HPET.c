#include "HPET.h"

#include "mem/mem.h"
#include "acpi/acpi.h"

static uint64_t hpet_base = 0;
static uint32_t femtoseconds_per_tick = 0;

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

void hpet_init(void) {
	struct hpet_table* table = acpi_find_sdt("HPET");
	if (!table) return;

	hpet_base = table->address.address + hhdm_offset;

	uint64_t caps = *(volatile uint64_t*)(hpet_base + HPET_REG_CAPS);
	femtoseconds_per_tick = (uint32_t)(caps >> 32);

	*(volatile uint64_t*)(hpet_base + HPET_REG_CONFIG) = 0x01;
}

uint64_t hpet_get_nanos(void) {
	if (hpet_base == 0) return 0;
	return (hpet_get_ticks() * femtoseconds_per_tick) / 1000000ULL;
}

uint64_t hpet_get_ms(void) {
	if (hpet_base == 0) return 0;
	return (hpet_get_ticks() * femtoseconds_per_tick) / 1000000000000ULL;
}

uint32_t hpet_femto_per_tick(void) {
	if (hpet_base == 0) return 0;
	uint64_t caps = *(volatile uint64_t*)(hpet_base + HPET_REG_CAPS);
	return (uint32_t)(caps >> 32);
}

uint64_t hpet_get_ticks(void) {
	return *(volatile uint64_t*)(hpet_base + 0xF0);
}

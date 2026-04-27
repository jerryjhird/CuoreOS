#include "HPET.h"

#include "mem/mem.h"
#include "mem/paging.h"
#include <stdint.h>
#include "builtinabs.h"

static uint64_t hpet_base = 0;
static uint32_t femtoseconds_per_tick = 0;

void hpet_init(void) {
	uintptr_t pml4_phys = vmm_get_pml4();
	uint64_t* pml4_virt = (uint64_t*)(pml4_phys + hhdm_offset);

	struct hpet_table* table = (struct hpet_table*)acpi_find_sdt("HPET");
	if (!table) return;

	uintptr_t hpet_phys = table->address.address;
	hpet_base = hpet_phys + hhdm_offset;

	vmm_map_page(pml4_virt, hpet_base, hpet_phys, PTE_PRESENT | PTE_WRITABLE | PTE_WRITE_THROUGH | PTE_CACHE_DISABLE | PTE_TYPE_DRIVER);

	uint64_t caps = *(volatile uint64_t*)(hpet_base + HPET_REG_CAPS);
	femtoseconds_per_tick = (uint32_t)(caps >> 32);

	*(volatile uint64_t*)(hpet_base + HPET_REG_CONFIG) |= 0x01;
}

uint64_t hpet_get_nanos(void) {
	if (UNLIKELY(hpet_base == 0)) return 0;
	return (hpet_get_ticks() * femtoseconds_per_tick) / 1000000ULL;
}

uint64_t hpet_get_ms(void) {
	if (UNLIKELY(hpet_base == 0)) return 0;
	return (hpet_get_ticks() * femtoseconds_per_tick) / 1000000000000ULL;
}

uint32_t hpet_femto_per_tick(void) {
	if (UNLIKELY(hpet_base == 0)) return 0;
	uint64_t caps = *(volatile uint64_t*)(hpet_base + HPET_REG_CAPS);
	return (uint32_t)(caps >> 32);
}

uint64_t hpet_get_ticks(void) {
	return *(volatile uint64_t*)(hpet_base + 0xF0);
}

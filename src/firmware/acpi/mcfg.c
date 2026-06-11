#include "mcfg.h"
#include "acpi.h"
#include "logbuf.h"
#include <stddef.h>
#include "mem/mem.h"
#include "abs.h"

static struct mcfg_table* mcfg_ptr = NULL;
static size_t mcfg_entry_count = 0;
static struct mcfg_entry* primary_entry = NULL;
bool mcfg_is_initialized = false;

void mcfg_init(void) {
	mcfg_ptr = (struct mcfg_table*)acpi_find_sdt("MCFG");

	if (UNLIKELY(!mcfg_ptr)) {
		logbuf_warn("[ MCFG ] table not found. PCIe ECAM will be unavailable!!!\n");
		return;
	}

	size_t table_size = mcfg_ptr->header.length;
	mcfg_entry_count = (table_size - sizeof(struct mcfg_table)) / sizeof(struct mcfg_entry);

	if (LIKELY(mcfg_entry_count > 0 && mcfg_ptr->entries[0].segment_group == 0)) {
		primary_entry = &mcfg_ptr->entries[0];
	}

	mcfg_is_initialized = true;

	logbuf_ok("[ MCFG ] Initialized MCFG at %p, entries: %zu\n", (void*)mcfg_ptr, (size_t)mcfg_entry_count);
}

void* mcfg_get_device_addr(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func) {
	if (UNLIKELY(!mcfg_ptr)) return NULL;

	struct mcfg_entry* entry = NULL;

	if (LIKELY(primary_entry && segment == 0 && bus <= primary_entry->end_bus)) {
		entry = primary_entry;
	} else {
		for (size_t i = 0; i < mcfg_entry_count; i++) {
			if (mcfg_ptr->entries[i].segment_group == segment &&
				bus >= mcfg_ptr->entries[i].start_bus &&
				bus <= mcfg_ptr->entries[i].end_bus) {
				entry = &mcfg_ptr->entries[i];
				break;
			}
		}
	}

	if (UNLIKELY(!entry)) return NULL;

	uintptr_t base = (uintptr_t)entry->base_address + hhdm_offset;
	uint32_t bus_off = (uint32_t)(bus - entry->start_bus);

	return (void*)(base + ((bus_off << 20) |
						   ((uint32_t)slot << 15) |
						   ((uint32_t)func << 12)));
}

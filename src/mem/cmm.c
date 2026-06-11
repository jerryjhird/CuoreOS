#include "cmm.h"
#include "kstate.h"
#include <string.h>
#include "logbuf.h"

#define MAX_CMM_REGIONS 64

static struct cmm_region internal_map[MAX_CMM_REGIONS];
static size_t internal_count = 0;
static uintptr_t max_phys_addr = 0;
uintptr_t max_ram_addr = 0;

static cmm_type_t translate_type(uint64_t limine_type) {
	switch (limine_type) {
		case LIMINE_MEMMAP_USABLE: return CMM_USABLE;
		case LIMINE_MEMMAP_RESERVED: return CMM_RESERVED;
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return CMM_ACPI_RECLAIMABLE;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return CMM_BOOTLOADER_RECLAIMABLE;
		case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return CMM_KERNEL_AND_MODULES;
		default: return CMM_BAD;
	}
}

void cmm_init(void) {
	struct limine_memmap_response *resp = memmap_request.response;

	for (size_t i = 0; i < resp->entry_count && i < MAX_CMM_REGIONS; i++) {
		struct limine_memmap_entry *e = resp->entries[i];

		internal_map[i].base   = (uintptr_t)e->base;
		internal_map[i].length = (size_t)e->length;
		internal_map[i].type   = translate_type(e->type);

		internal_count++;

		uintptr_t end = (uintptr_t)(e->base + e->length);
		if (end > max_phys_addr) max_phys_addr = end;
	}

	for (size_t i = 0; i < internal_count; i++) {
		if (internal_map[i].type == CMM_USABLE) {
			uintptr_t end = internal_map[i].base + internal_map[i].length;
			if (end > max_ram_addr) max_ram_addr = end;
		}
	}

	logbuf_ok("[ CMM  ] CMM initialized successfully\n");
	logbuf_debug("[ CMM  ] Top of Physical address space: %p\n", (void*)max_phys_addr);
	logbuf_debug("[ CMM  ] Top of RAM: %p\n", (void*)max_ram_addr);
}

size_t cmm_get_region_count(void) { return internal_count; }
uintptr_t cmm_get_phys_top(void) { return max_phys_addr; }
uintptr_t cmm_get_ram_top(void) { return max_ram_addr; }

const struct cmm_region* cmm_get_region(size_t index) {
	if (index >= internal_count) return NULL;
	return &internal_map[index];
}

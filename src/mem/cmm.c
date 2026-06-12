#include "cmm.h"
#include "kstate.h"
#include "panic.h"
#include "logbuf.h"
#include "mem/mem.h"

static struct cmm_region *internal_map = NULL;
static size_t internal_capacity = 0;
static size_t internal_count = 0;

static uintptr_t max_phys_addr = 0;
static uintptr_t max_ram_addr = 0;

static cmm_type_t translate_type(uint64_t limine_type) {
	switch (limine_type) {
		case LIMINE_MEMMAP_USABLE: return CMM_USABLE;
		case LIMINE_MEMMAP_RESERVED: return CMM_RESERVED;
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return CMM_ACPI_RECLAIMABLE;
		case LIMINE_MEMMAP_ACPI_NVS: return CMM_ACPI_NVS;
		case LIMINE_MEMMAP_BAD_MEMORY: return CMM_BAD;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return CMM_BOOTLOADER_RECLAIMABLE;
		case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return CMM_KERNEL_AND_MODULES;
		case LIMINE_MEMMAP_FRAMEBUFFER: return CMM_FRAMEBUFFER;
		case LIMINE_MEMMAP_RESERVED_MAPPED: return CMM_RESERVED_MAPPED;
		default: return CMM_UNKNOWN;
	}
}

void cmm_init(void) {
    struct limine_memmap_response *resp = memmap_request.response;

    size_t required_slots = resp->entry_count * 2;
    size_t bytes_needed = (required_slots * sizeof(struct cmm_region) + 4095) & ~4095;

	// find a usable region for the CMM memmap
    for (size_t i = 0; i < resp->entry_count; i++) {
        if (resp->entries[i]->type == LIMINE_MEMMAP_USABLE && resp->entries[i]->length >= bytes_needed) {
            internal_map = (struct cmm_region*)((uintptr_t)resp->entries[i]->base + hhdm_offset);
            internal_capacity = required_slots;
            break;
        }
    }

    if (!internal_map) {
        panic("CMM", "CRITICAL: Failed to locate memory for internal map!\n");
    }

	// populate CMM memmap
    for (size_t i = 0; i < resp->entry_count; i++) {
        struct limine_memmap_entry *e = resp->entries[i];
        uintptr_t e_base = (uintptr_t)e->base;
        size_t e_len = (size_t)e->length;

        if (e_base == (uintptr_t)internal_map - hhdm_offset) {
            internal_map[internal_count++] = (struct cmm_region){e_base, bytes_needed, CMM_KERNEL_AND_MODULES};
            internal_map[internal_count++] = (struct cmm_region){e_base + bytes_needed, e_len - bytes_needed, CMM_USABLE};
        } else {
            internal_map[internal_count++] = (struct cmm_region){e_base, e_len, translate_type(e->type)};
        }

        uintptr_t end = internal_map[internal_count-1].base + internal_map[internal_count-1].length;
        if (end > max_phys_addr) max_phys_addr = end;
        if (internal_map[internal_count-1].type == CMM_USABLE && end > max_ram_addr) max_ram_addr = end;
    }

    logbuf_ok("[ CMM  ] CMM initialized successfully\n");
    logbuf_debug("[ CMM  ] Total Regions: %zu\n", internal_count);
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

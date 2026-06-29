#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

typedef enum {
	CMM_USABLE,
	CMM_RESERVED,
	CMM_ACPI_RECLAIMABLE,
	CMM_ACPI_NVS,
	CMM_BOOTLOADER_RECLAIMABLE,
	CMM_KERNEL_EARLY,
	CMM_KERNEL_AND_MODULES,
	CMM_FRAMEBUFFER,
	CMM_RESERVED_MAPPED,
	CMM_UNKNOWN,
	CMM_BAD,
} cmm_type_t;

struct cmm_region {
	uint64_t id;
	uintptr_t base;
	size_t length;
	cmm_type_t type;
};

void cmm_init(void);
void cmm_migrate_to_heap(void);

void cmm_insert_region(uintptr_t base, size_t length, cmm_type_t new_type);
const struct cmm_region* cmm_get_region(uint64_t id);
const struct cmm_region* cmm_get_region_by_index(size_t index);

size_t cmm_get_region_count(void);
uintptr_t cmm_get_phys_top(void);
uintptr_t cmm_get_ram_top(void);

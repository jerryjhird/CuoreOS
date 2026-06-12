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
	CMM_KERNEL_AND_MODULES,
	CMM_FRAMEBUFFER,
	CMM_RESERVED_MAPPED,
	CMM_UNKNOWN,
	CMM_BAD,
} cmm_type_t;

struct cmm_region {
	uintptr_t base;
	size_t length;
	cmm_type_t type;
};

void cmm_init(void);

size_t cmm_get_region_count(void);
const struct cmm_region* cmm_get_region(size_t index);
uintptr_t cmm_get_phys_top(void);
uintptr_t cmm_get_ram_top(void);

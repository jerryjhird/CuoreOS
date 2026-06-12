#pragma once
#include "acpi.h"

typedef struct acpi_waet_header {
	acpi_sdt_header_t header;
	uint32_t flags;
} __attribute__((packed)) acpi_waet_t;

#define ACPI_WAET_RTC_CMOS_ADDRESS_OUT_OF_INDEX (1 << 0)
#define ACPI_WAET_ACPI_PWR_MANAGEMENT_PRESENT (1 << 1)

void waet_init(struct acpi_sdt_header* table);

#pragma once

#include <stdint.h>
#include "acpi.h"

struct bgrt_table {
	struct acpi_sdt_header header;
	uint16_t version;
	uint8_t status;
	uint8_t image_type;
	uint64_t image_address;
	uint32_t image_offset_x;
	uint32_t image_offset_y;
} __attribute__((packed));

enum bgrt_image_type {
	BGRT_IMAGE_TYPE_BMP = 0,
	BGRT_IMAGE_TYPE_RESERVED = 1 // 1 - 255
};

void bgrt_init(struct acpi_sdt_header* acpi_tab);

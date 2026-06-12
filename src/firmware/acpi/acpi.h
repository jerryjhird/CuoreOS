#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct acpi_sdt_header {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct acpi_gas {
	uint8_t  address_space_id;
	uint8_t  register_bit_width;
	uint8_t  register_bit_offset;
	uint8_t  access_size;
	uint64_t address;
} __attribute__((packed)) acpi_gas_t;

struct rsdp_v2 {
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum;
	uint8_t reserved[3];
} __attribute__((packed));

void acpi_init(void);
bool acpi_checksum(struct acpi_sdt_header* table);

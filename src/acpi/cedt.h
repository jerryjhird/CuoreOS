#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "acpi.h"

#define ACPI_CEDT_TYPE_CHBS 0
#define ACPI_CEDT_TYPE_CFMWS 1
#define ACPI_CEDT_TYPE_CXIMS 2
#define ACPI_CEDT_TYPE_RDPAS 3

#define ACPI_CEDT_CHBS_UID_FORMAT_INTEGER  (0 << 0)
#define ACPI_CEDT_CHBS_UID_FORMAT_STRING   (1 << 0)

typedef struct acpi_cedt_header {
	uint8_t type;
	uint8_t reserved;
	uint16_t length;
} __attribute__((packed)) acpi_cedt_header_t;

typedef struct acpi_cedt_chbs {
	acpi_cedt_header_t header;
	uint32_t local_uid;
	uint32_t flags;
	uint64_t cxl_version;
	uint64_t reserved;
	uint64_t base_address;
	uint64_t length;
} __attribute__((packed)) acpi_cedt_chbs_t;

#define ACPI_CEDT_CFMWS_RESTRICT_TYPE2 (1 << 0) // 0x1
#define ACPI_CEDT_CFMWS_RESTRICT_TYPE3 (1 << 1) // 0x2
#define ACPI_CEDT_CFMWS_RESTRICT_CXL_PMEM (1 << 2) // 0x4
#define ACPI_CEDT_CFMWS_RESTRICT_CXL_RAM (1 << 3) // 0x8

typedef struct acpi_cedt_cfmws {
	acpi_cedt_header_t header;
	uint32_t reserved1;
	uint64_t window_base;
	uint64_t window_size;
	uint32_t window_attr;
	uint32_t interconnect_element_ways;
	uint16_t reserved2;
	uint16_t target_restrict;

	uint32_t target_windows[];
} __attribute__((packed)) acpi_cedt_cfmws_t;

typedef struct acpi_cedt_cxims {
	acpi_cedt_header_t header;
	uint16_t reserved1;
	uint16_t hbm_id;
	uint32_t xormatrix_ways;
	uint64_t xormatrix_value;
} __attribute__((packed)) acpi_cedt_cxims_t;

typedef struct acpi_cedt_rdpas {
	acpi_cedt_header_t header;
	uint8_t flags;
	uint8_t reserved1;
	uint16_t local_hmem_dom;
	char legal_regulatory_cc[2];
	uint16_t reserved2;
	uint64_t qos_telemetry_base;
	uint64_t qos_telemetry_size;
} __attribute__((packed)) acpi_cedt_rdpas_t;

typedef struct {
	uint64_t phys_base;
	uint64_t size;
	uint32_t host_bridge_uid;
	bool found;
} cedt_ret_t;

void parse_cedt(acpi_sdt_header_t* table, cedt_ret_t* match);

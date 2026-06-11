#pragma once

#include "firmware/smbios/smbios.h"

struct smbios_type17_memory_device {
	struct smbios_header header;

	uint16_t physical_memory_array_handle;
	uint16_t memory_error_information_handle;
	uint16_t total_width;
	uint16_t data_width;
	uint16_t size;
	uint8_t form_factor;
	uint8_t device_set;
	uint8_t device_locator_str_idx;
	uint8_t bank_locator_str_idx;
	uint8_t memory_type;
	uint16_t type_detail;
	uint16_t speed;
	uint8_t manufacturer_str_idx;
	uint8_t serial_number_str_idx;
	uint8_t asset_tag_str_idx;
	uint8_t part_number_str_idx;

	// SMBIOS 2.7+ fields
	// uint8_t  attributes;
	// uint32_t extended_size;
	// uint16_t configured_memory_speed;
} __attribute__((packed));

enum smbios_type17_memory_type {
	SMBIOS_T17_MEM_UNKNOWN = 0x02,
	SMBIOS_T17_MEM_DRAM = 0x03,
	SMBIOS_T17_MEM_DDR3 = 0x18,
	SMBIOS_T17_MEM_DDR4 = 0x1A,
	SMBIOS_T17_MEM_DDR5 = 0x22,
	SMBIOS_T17_MEM_CXL = 0x25
};

inline const char* smbios_type17_get_type_str(uint8_t type) {
	switch (type) {
		case SMBIOS_T17_MEM_DDR3: return "DDR3";
		case SMBIOS_T17_MEM_DDR4: return "DDR4";
		case SMBIOS_T17_MEM_DDR5: return "DDR5";
		case SMBIOS_T17_MEM_CXL: return "CXL Expansion Media";
		case SMBIOS_T17_MEM_DRAM: return "Standard DRAM";
		default: return "Unknown/Other";
	}
}

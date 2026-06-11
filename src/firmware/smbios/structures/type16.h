#pragma once

#include "firmware/smbios/smbios.h"

struct smbios_type16_memory_array {
	struct smbios_header header;

	uint8_t  location;
	uint8_t  use;
	uint8_t  memory_error_correction;
	uint32_t maximum_capacity;
	uint16_t memory_error_information_handle;
	uint16_t memory_devices_count;

	// SMBIOS 2.7+ fields
	// uint64_t extended_maximum_capacity;
} __attribute__((packed));

enum smbios_type16_location {
	SMBIOS_T16_LOC_OTHER = 0x01,
	SMBIOS_T16_LOC_UNKNOWN = 0x02,
	SMBIOS_T16_LOC_MOTHERBOARD = 0x03,
	SMBIOS_T16_LOC_ISA_COMPAT_CARD = 0x04,
	SMBIOS_T16_LOC_EISA_COMPAT_CARD = 0x05,
	SMBIOS_T16_LOC_PCI_COMPAT_CARD = 0x06,
	SMBIOS_T16_LOC_MCA_COMPAT_CARD = 0x07,
	SMBIOS_T16_LOC_PCMCIA_CARD = 0x08,
	SMBIOS_T16_LOC_PROPRIETARY_CARD = 0x09,
	SMBIOS_T16_LOC_NVDIMM_CARD = 0x0A
};

enum smbios_type16_ecc {
	SMBIOS_T16_ECC_OTHER = 0x01,
	SMBIOS_T16_ECC_UNKNOWN = 0x02,
	SMBIOS_T16_ECC_NONE = 0x03,
	SMBIOS_T16_ECC_PARITY  = 0x04,
	SMBIOS_T16_ECC_SINGLE_BIT = 0x05,
	SMBIOS_T16_ECC_MULTI_BIT = 0x06,
	SMBIOS_T16_ECC_CRC = 0x07
};

inline const char* smbios_type16_get_location_str(uint8_t location) {
	switch (location) {
		case 0x01: return "Other";
		case 0x02: return "Unknown";
		case 0x03: return "System Board (Motherboard)";
		case 0x04: return "ISA Compatibility Card";
		case 0x05: return "EISA Compatibility Card";
		case 0x06: return "PCI Compatibility Card";
		case 0x07: return "MCA Compatibility Card";
		case 0x08: return "PCMCIA Card";
		case 0x09: return "Proprietary Card";
		case 0x0A: return "CXL / NVDIMM Card";
		default: return "Reserved / Out of Spec";
	}
}

inline const char* smbios_type16_get_ecc_str(uint8_t ecc) {
	switch (ecc) {
		case 0x01: return "Other";
		case 0x02: return "Unknown";
		case 0x03: return "None (Non-ECC)";
		case 0x04: return "Parity";
		case 0x05: return "Single-bit ECC";
		case 0x06: return "Multi-bit ECC";
		case 0x07: return "CRC";
		default: return "Reserved";
	}
}

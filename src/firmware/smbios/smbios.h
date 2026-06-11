#pragma once

#include <stdint.h>
#include <stddef.h>

struct smbios_header {
	uint8_t  type;
	uint8_t  length;
	uint16_t handle;
} __attribute__((packed));

struct smbios2_eps {
	char anchor[4]; // "_SM_"
	uint8_t checksum;
	uint8_t length;
	uint8_t major_version;
	uint8_t minor_version;
	uint16_t max_structure_size;
	uint8_t eps_revision;
	uint8_t formatted_area[5];
	char dmi_anchor[5]; // "_DMI_"
	uint8_t dmi_checksum;
	uint16_t table_length;
	uint32_t table_address; // 32 bit phys address
	uint16_t structure_count;
	uint8_t bcd_revision;
} __attribute__((packed));

struct smbios3_eps {
	char anchor[5]; // "_SM3_"
	uint8_t  checksum;
	uint8_t  length;
	uint8_t  major_version;
	uint8_t  minor_version;
	uint8_t  doc_rev;
	uint8_t  eps_revision;
	uint8_t  reserved;
	uint32_t max_structure_size;
	uint64_t table_address; // 64 bit phys address
} __attribute__((packed));

void smbios_init(void *entry_32, void *entry_64);
const char* smbios_get_string(const struct smbios_header *header, uint8_t idx);
const struct smbios_header* smbios_get_by_type(uint8_t type, size_t index);
const struct smbios_header* smbios_get_by_handle(uint16_t handle);

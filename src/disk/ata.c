#include "ata.h"
#include "mem/mem.h"

static void ata_parse_string(char* dest, const uint16_t* id_data, int word_start, int word_count) {
	for (int i = 0; i < word_count; i++) {
		uint16_t val = id_data[word_start + i];
		dest[i * 2]	 = (char)(val >> 8);
		dest[i * 2 + 1] = (char)(val & 0xFF);
	}
	dest[word_count * 2] = '\0';
	for (int i = (word_count * 2) - 1; i >= 0 && dest[i] == ' '; i--) {
		dest[i] = '\0';
	}
}

ata_identity_t ata_identify(const uint16_t* id_data) {
	ata_identity_t info;
	memset(&info, 0, sizeof(info));

	info.config = id_data[0];

	if (id_data[83] & (1 << 10)) {
		info.total_sectors = ((uint64_t)id_data[103] << 48) | ((uint64_t)id_data[102] << 32) | ((uint64_t)id_data[101] << 16) | ((uint64_t)id_data[100]);
	} else {
		info.total_sectors = ((uint32_t)id_data[61] << 16) | ((uint32_t)id_data[60]);
	}

	ata_parse_string(info.model, id_data, 27, 20);
	ata_parse_string(info.serial, id_data, 10, 10);
	ata_parse_string(info.firmware, id_data, 23, 4);

	info.capabilities = id_data[49];
	info.field_validity = id_data[53];
	info.pio_modes = id_data[64];
	info.dma_modes = ((uint32_t)id_data[63] << 16) | id_data[88];
	info.command_sets_82 = id_data[82];
	info.command_sets_83 = id_data[83];
	info.logical_sector_size = id_data[106] & 0x0FFF;

	return info;
}

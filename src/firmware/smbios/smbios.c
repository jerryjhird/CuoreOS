#include "smbios.h"
#include "logbuf.h"

static uintptr_t smbios_table_base = 0;
static uint8_t smbios_major = 0;
static uint8_t smbios_minor = 0;
static bool smbios_available = false;

const char* smbios_get_string(const struct smbios_header *header, uint8_t idx) {
	if (idx == 0) return "None";

	const char *str_table = (const char *)header + header->length;
	uint8_t current_idx = 1;

	while (current_idx < idx) {
		while (*str_table != '\0') {
			str_table++;
		}
		str_table++;
		current_idx++;

		if (*str_table == '\0') return "Unknown";
	}

	return str_table;
}

const struct smbios_header* smbios_get_by_type(uint8_t type, size_t index) {
	if (!smbios_available || !smbios_table_base) return NULL;

	uintptr_t current_addr = smbios_table_base;
	size_t match_count = 0;

	while (1) {
		const struct smbios_header *header = (const struct smbios_header *)current_addr;

		if (header->type == type) {
			if (match_count == index) {
				return header;
			}
			match_count++;
		}

		if (header->type == 127) break;

		uintptr_t next_addr = current_addr + header->length;
		while (*(uint16_t *)next_addr != 0x0000) {
			next_addr++;
		}
		current_addr = next_addr + 2;
	}

	return NULL;
}

const struct smbios_header* smbios_get_by_handle(uint16_t handle) {
	if (!smbios_available || !smbios_table_base) return NULL;

	uintptr_t current_addr = smbios_table_base;

	while (1) {
		const struct smbios_header *header = (const struct smbios_header *)current_addr;

		if (header->handle == handle) {
			return header;
		}

		if (header->type == 127) break;

		uintptr_t next_addr = current_addr + header->length;
		while (*(uint16_t *)next_addr != 0x0000) {
			next_addr++;
		}
		current_addr = next_addr + 2;
	}

	return NULL;
}

void smbios_init(void *entry_32, void *entry_64) {
	// check for 64 bit entry point first
	if (entry_64 != NULL) {
		struct smbios3_eps *eps64 = (struct smbios3_eps *)entry_64;
		if (eps64->anchor[0] == '_' && eps64->anchor[1] == 'S' &&
			eps64->anchor[2] == 'M' && eps64->anchor[3] == '3' && eps64->anchor[4] == '_') {

			smbios_table_base = (uintptr_t)eps64->table_address;
			smbios_major = eps64->major_version;
			smbios_minor = eps64->minor_version;
			smbios_available = true;

			logbuf_ok("[SMBIOS] parser ready via 64 bit table (v%d.%d)\n", smbios_major, smbios_minor);
		}
	}

	// 32 bit entry point?
	if (!smbios_available && entry_32 != NULL) {
		struct smbios2_eps *eps32 = (struct smbios2_eps *)entry_32;
		if (eps32->anchor[0] == '_' && eps32->anchor[1] == 'S' &&
			eps32->anchor[2] == 'M' && eps32->anchor[3] == '_') {

			smbios_table_base = (uintptr_t)eps32->table_address;
			smbios_major = eps32->major_version;
			smbios_minor = eps32->minor_version;
			smbios_available = true;

			logbuf_ok("[SMBIOS] parser is ready via 32 bit table (v%d.%d)\n", smbios_major, smbios_minor);
		}
	}

	// log every found structure
	#ifdef DEBUG
	if (smbios_available && smbios_table_base != 0) {
		uintptr_t current_addr = smbios_table_base;

		while (1) {
			const struct smbios_header *header = (const struct smbios_header *)current_addr;

			if (header->type == 127) {
				break;
			}

			if (header->type != 126) {
				logbuf_debug("[SMBIOS] Found Structure: %d | handle: 0x%X | length: %d bytes\n", header->type, header->handle, header->length);
			}

			uintptr_t next_addr = current_addr + header->length;
			while (*(uint16_t *)next_addr != 0x0000) {
				next_addr++;
			}

			current_addr = next_addr + 2;
		}
		return;
	}
	#endif

	if (!smbios_available) {
		logbuf_warn("[SMBIOS] no valid 32 bit or 64 bit table anchors found, SMBIOS parsing will not be available\n");
	}
}

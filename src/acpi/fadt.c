#include "fadt.h"
#include "logbuf.h"
#include "mem/mem.h"

acpi_fadt_t* fadt = NULL;

void fadt_init(void) {
	fadt = (acpi_fadt_t*)acpi_find_sdt("FACP");

	if (!fadt) {
		logbuf_write("[ FADT ] FADT is missing or corrupted!\n");
		return;
	}

	logbuf_printf("[ FADT ] Found FADT at %p\n", (void*)fadt);
}

uintptr_t fadt_get_pm1a_cnt(void) {
	if (!fadt) return 0;

	if (fadt->header.length >= 244 && fadt->x_pm1a_cnt_blk.address != 0) {
		return (uintptr_t)fadt->x_pm1a_cnt_blk.address;
	}

	return (uintptr_t)fadt->pm1a_cnt_blk;
}

bool fadt_is_pm1a_mmio(void) {
	if (!fadt) return false;

	if (fadt->header.length >= 244 && fadt->x_pm1a_cnt_blk.address != 0) {
		return fadt->x_pm1a_cnt_blk.address_space_id == 0;
	}

	return false;
}

uint16_t fadt_get_s5_types(void) {
	if (!fadt) return (5 << 10); // fallback

	uint8_t* dsdt = NULL;
	if (fadt->header.length >= 148 && fadt->x_dsdt != 0) {
		dsdt = (uint8_t*)(fadt->x_dsdt + hhdm_offset);
	} else {
		dsdt = (uint8_t*)((uintptr_t)fadt->dsdt + hhdm_offset);
	}

	uint32_t dsdt_len; memcpy(&dsdt_len, dsdt + 4, sizeof(uint32_t));

	for (uint32_t i = 0; i < dsdt_len - 8; i++) {
		if (memcmp(dsdt + i, "_S5_", 4) == 0) {
			i += 4;
			if (dsdt[i] == 0x12) {
				i += 1;
				uint8_t pkg_len = dsdt[i];
				if ((pkg_len >> 6) == 0) i += 1;
				else if ((pkg_len >> 6) == 1) i += 2;
				else if ((pkg_len >> 6) == 2) i += 3;
				else i += 4;

				i += 1;

				return (uint16_t)dsdt[i] << 10;
			}
		}
	}

	return (5 << 10); // if not found
}

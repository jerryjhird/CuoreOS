#include "acpi.h"

#include "mem/mem.h"
#include "abs.h"
#include "logbuf.h"

#include "madt.h"
#include "fadt.h"
#include "mcfg.h"
#include "cedt.h"
#include "hpet.h"
#include "waet.h"
#include "bgrt.h"
#include <stdint.h>

typedef void (*acpi_dtable_handler)(struct acpi_sdt_header* table);
#define TO_CHAR_ARRAY(s) { (s)[0], (s)[1], (s)[2], (s)[3] }

struct acpi_dtable_entry {
	char signature[4];
	acpi_dtable_handler handler;
};

static const struct acpi_dtable_entry acpi_discovery_table[] = {
	{ TO_CHAR_ARRAY("APIC"), madt_init },
	{ TO_CHAR_ARRAY("FACP"), fadt_init },
	{ TO_CHAR_ARRAY("MCFG"), mcfg_init },
	{ TO_CHAR_ARRAY("CEDT"), cedt_init },
	{ TO_CHAR_ARRAY("HPET"), hpet_init },
	{ TO_CHAR_ARRAY("WAET"), waet_init },
	{ TO_CHAR_ARRAY("BGRT"), bgrt_init }
};

static const size_t acpi_discovery_table_c = sizeof(acpi_discovery_table) / sizeof(struct acpi_dtable_entry);

static struct acpi_sdt_header* xsdt = NULL;
static bool is_xsdt = false;

bool acpi_checksum(struct acpi_sdt_header* table) {
	uint8_t sum = 0;
	uint8_t* bytes = (uint8_t*)table;

	for (uint32_t i = 0; i < table->length; i++) {
		sum += bytes[i];
	}

	return (sum == 0);
}

void acpi_init(uintptr_t rsdp_phys) {
	struct rsdp_v2* rsdp = (struct rsdp_v2*)rsdp_phys;

	if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
		xsdt = (struct acpi_sdt_header*)(rsdp->xsdt_address + hhdm_offset);
		is_xsdt = true;
	} else {
		xsdt = (struct acpi_sdt_header*)((uint64_t)rsdp->rsdt_address + hhdm_offset);
		is_xsdt = false;
	}

	if (UNLIKELY(!xsdt)) return;

	size_t ptr_size = is_xsdt ? 8 : 4;
	int entries = (xsdt->length - sizeof(struct acpi_sdt_header)) / ptr_size;
	uint8_t* table_ptrs_base = (uint8_t*)xsdt + sizeof(struct acpi_sdt_header);

	for (int i = 0; i < entries; i++) {
		uint64_t table_phys = 0;
		uint8_t* entry_ptr = table_ptrs_base + (i * ptr_size);

		if (is_xsdt) {
			memcpy(&table_phys, entry_ptr, 8);
		} else {
			uint32_t table_phys_32;
			memcpy(&table_phys_32, entry_ptr, 4);
			table_phys = table_phys_32;
		}

		if (table_phys == 0) continue;

		struct acpi_sdt_header* table = (struct acpi_sdt_header*)(table_phys + hhdm_offset);

		bool ok = acpi_checksum(table);

		char sig[5] = {0};
		memcpy(sig, table->signature, 4);

		if (ok) {
			logbuf_debug("[ ACPI ] Found Table: %s at %p (Checksum: OK)\n", sig, (void*)table);
		} else {
			logbuf_warn("[ ACPI ] Invalid Table: %s at %p (Checksum: FAILED)\n", sig, (void*)table);
			continue; // skip init
		}

		for (size_t j = 0; j < acpi_discovery_table_c; j++) {
			if (memcmp(table->signature, acpi_discovery_table[j].signature, 4) == 0) {
				acpi_discovery_table[j].handler(table);
				break;
			}
		}
	}
}

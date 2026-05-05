#include "acpi.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "kstate.h"
#include "abs.h"
#include "logbuf.h"

static struct acpi_sdt_header* xsdt = NULL;
static bool is_xsdt = false;

void acpi_init(void) {
	struct limine_rsdp_response* resp = rsdp_request.response;
	struct rsdp_v2* rsdp = (struct rsdp_v2*)resp->address;

	if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
		xsdt = (struct acpi_sdt_header*)(rsdp->xsdt_address + hhdm_offset);
		is_xsdt = true;
	} else {
		xsdt = (struct acpi_sdt_header*)((uint64_t)rsdp->rsdt_address + hhdm_offset);
		is_xsdt = false;
	}
}

static bool acpi_checksum(struct acpi_sdt_header* table) {
	uint8_t sum = 0;
	uint8_t* bytes = (uint8_t*)table;

	for (uint32_t i = 0; i < table->length; i++) {
		sum += bytes[i];
	}

	return (sum == 0);
}

void* acpi_find_sdt(const char* signature) {
	if (UNLIKELY(!xsdt)) return NULL;

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

		if (memcmp(table->signature, signature, 4) == 0) {
			if (!acpi_checksum(table)) {
				logbuf_write("[ ACPI ] Checksum failed for table ");
				logbuf_write(signature);
				logbuf_write("\n");
				return NULL;
			}

			return (void*)table;
		}
	}
	return NULL;
}

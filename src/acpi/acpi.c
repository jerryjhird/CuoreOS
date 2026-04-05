#include "acpi.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "kstate.h"

static struct acpi_sdt_header* xsdt = NULL;

void acpi_init(void) {
	struct limine_rsdp_response* resp = rsdp_request.response;
	struct rsdp_v2* rsdp = (struct rsdp_v2*)resp->address;

	if (rsdp->revision >= 2) {
		xsdt = (struct acpi_sdt_header*)(rsdp->xsdt_address + hhdm_offset);
	} else {
		xsdt = (struct acpi_sdt_header*)((uint64_t)rsdp->rsdt_address + hhdm_offset);
	}
}

void* acpi_find_sdt(const char* signature) {
	if (!xsdt) return NULL;

	int entries = (xsdt->length - sizeof(struct acpi_sdt_header)) / 8;
	uint64_t* table_ptrs = (uint64_t*)((uint8_t*)xsdt + sizeof(struct acpi_sdt_header));

	for (int i = 0; i < entries; i++) {
		struct acpi_sdt_header* table = (struct acpi_sdt_header*)(table_ptrs[i] + hhdm_offset);

		if (memcmp(table->signature, signature, 4) == 0) {
			return (void*)table;
		}
	}

	return NULL;
}

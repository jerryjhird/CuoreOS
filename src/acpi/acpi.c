#include "acpi.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "kstate.h"

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

void* acpi_find_sdt(const char* signature) {
	if (!xsdt) return NULL;

	size_t ptr_size = is_xsdt ? 8 : 4;
	int entries = (xsdt->length - sizeof(struct acpi_sdt_header)) / ptr_size;
	uint8_t* table_ptrs_base = (uint8_t*)xsdt + sizeof(struct acpi_sdt_header);

	for (int i = 0; i < entries; i++) {
		uint64_t table_phys;
		if (is_xsdt) {
			table_phys = ((uint64_t*)table_ptrs_base)[i];
		} else {
			table_phys = ((uint32_t*)table_ptrs_base)[i];
		}

		struct acpi_sdt_header* table = (struct acpi_sdt_header*)(table_phys + hhdm_offset);
		if (memcmp(table->signature, signature, 4) == 0) {
			return (void*)table;
		}
	}
	return NULL;
}

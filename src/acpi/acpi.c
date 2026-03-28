#include "acpi.h"
#include "mem/mem.h"
#include "kstate.h"

static struct acpi_sdt_header* xsdt = NULL;

void acpi_init(void) {
    struct limine_rsdp_response* rsdp = rsdp_request.response;
    if (!rsdp || !rsdp->address) {
        panic("ACPI", "RSDP not found! Is ACPI enabled in firmware?");
    }

    uint64_t* xsdt_phys_ptr = (uint64_t*)((uint8_t*)rsdp->address + 24);
    xsdt = (struct acpi_sdt_header*)(*xsdt_phys_ptr + hhdm_offset);
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
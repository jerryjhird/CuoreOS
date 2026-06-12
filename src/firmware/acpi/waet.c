#include "waet.h"
#include "logbuf.h"

void waet_init(struct acpi_sdt_header* table) {
	acpi_waet_t* waet = (acpi_waet_t*)table;

	if (waet->flags & ACPI_WAET_RTC_CMOS_ADDRESS_OUT_OF_INDEX) {
		logbuf_info("[ ACPI ] WAET reports RTC CMOS at non-standard index\n");
	}

	if (waet->flags & ACPI_WAET_ACPI_PWR_MANAGEMENT_PRESENT) {
		logbuf_info("[ ACPI ] WAET reports ACPI Power Management supported\n");
	}
}

#include "RTC.h"

#include "CMOS.h"
#include "acpi/fadt.h"
#include <stddef.h>

#define BCD_TO_BIN(val) (((val) & 0x0F) + (((val) / 16) * 10))

static int is_updating(void) {
	return (cmos_read(0x0A) & 0x80);
}

void read_rtc(rtc_time_t *time) {
	uint8_t century = 0;
	uint8_t registerB;

	while (is_updating());

	time->second = cmos_read(0x00);
	time->minute = cmos_read(0x02);
	time->hour   = cmos_read(0x04);
	time->day	= cmos_read(0x07);
	time->month  = cmos_read(0x08);
	time->year   = cmos_read(0x09);

	if (fadt != NULL && fadt->century != 0) {
		century = cmos_read(fadt->century);
	}

	registerB = cmos_read(0x0B);

	if (!(registerB & 0x04)) {
		time->second = BCD_TO_BIN(time->second);
		time->minute = BCD_TO_BIN(time->minute);
		time->hour   = ((time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10)) | (time->hour & 0x80);
		time->day	= BCD_TO_BIN(time->day);
		time->month  = BCD_TO_BIN(time->month);
		time->year   = BCD_TO_BIN(time->year);

		if (fadt != NULL && fadt->century != 0) {
			century = BCD_TO_BIN(century);
		}
	}

	// handle 12 hour mode
	if (!(registerB & 0x02) && (time->hour & 0x80)) {
		time->hour = ((time->hour & 0x7F) + 12) % 24;
	}

	// final year calculation with FADT century offset
	if (fadt != NULL && fadt->century != 0 && century != 0) {
		time->year += (uint32_t)century * 100;
	} else {
		// fallback
		time->year += 2000;
	}
}

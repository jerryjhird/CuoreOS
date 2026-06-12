#include "_time.h"
#include <stdint.h>
#include "devices/char.h"
#include "drivers/RTC.h"
#include "firmware/acpi/hpet.h"
#include "abs.h"

static time_t boot_epoch = 0;
static uint64_t boot_hpet_ticks = 0;
static uint64_t hpet_ticks_per_ms = 0;
static uint64_t hpet_ticks_per_us = 0;

static const int days_before_month[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static uint64_t get_epoch_rtc(void) {
	rtc_time_t t1, t2;

	do {
		read_rtc(&t1);
		read_rtc(&t2);
	} while (t1.second != t2.second || t1.minute != t2.minute ||
			 t1.hour   != t2.hour   || t1.day	!= t2.day	||
			 t1.month  != t2.month  || t1.year   != t2.year);

	uint64_t year = t1.year;

	uint64_t total_days = (year - 1970) * 365;
	total_days += (year - 1969) / 4;
	total_days -= (year - 1901) / 100;
	total_days += (year - 1601) / 400;

	total_days += days_before_month[t1.month - 1];

	if (t1.month > 2) {
		if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
			total_days++;
		}
	}

	total_days += (t1.day - 1);

	return (total_days * 86400) + (t1.hour * 3600) + (t1.minute * 60) + t1.second;
}

// adjust RTC using HPET
void time_sync(void) {
	rtc_time_t start_time, current_time;

	read_rtc(&start_time);
	current_time = start_time;

	while (current_time.second == start_time.second) {
		__asm__ volatile("pause");
		read_rtc(&current_time);
	}

	boot_hpet_ticks = hpet_get_ticks();
	boot_epoch = get_epoch_rtc();

	dev_puts(debug_dev, "[ TIME ] Time synced\n");
}

void time_init(void) {
	uint64_t f_per_tick = hpet_femto_per_tick();

	hpet_ticks_per_ms = 1000000000000ULL / f_per_tick;
	hpet_ticks_per_us = 1000000000ULL / f_per_tick;

	boot_hpet_ticks = hpet_get_ticks();
	boot_epoch = get_epoch_rtc();
}

time_t get_epoch(void) {
	if (UNLIKELY(boot_hpet_ticks == 0)) return boot_epoch;

	uint64_t elapsed_ticks = hpet_get_ticks() - boot_hpet_ticks;
	uint64_t f_per_tick = hpet_femto_per_tick();
	uint64_t f_per_sec = 1000000000000000ULL;

	uint64_t ticks_per_sec = f_per_sec / f_per_tick;
	uint64_t elapsed_sec = elapsed_ticks / ticks_per_sec;

	return boot_epoch + (time_t)elapsed_sec;
}

//
// sleep functions
//

void msleep(uint64_t ms) {
	uint64_t total_ticks = ms * hpet_ticks_per_ms;
	uint64_t start = hpet_get_ticks();

	while ((hpet_get_ticks() - start) < total_ticks) {
		__asm__ volatile("pause");
	}
}

void usleep(uint64_t us) {
	uint64_t total_ticks = us * hpet_ticks_per_us;
	uint64_t start = hpet_get_ticks();

	while ((hpet_get_ticks() - start) < total_ticks) {
		__asm__ volatile("pause");
	}
}

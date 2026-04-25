#include "_time.h"
#include <stdint.h>
#include "devices.h"
#include "drivers/RTC.h"
#include "drivers/HPET.h"
#include "builtinabs.h"

static time_t boot_epoch = 0;
static uint64_t boot_hpet_ticks = 0;
static uint64_t hpet_ticks_per_ms = 0;
static uint64_t hpet_ticks_per_us = 0;

static const int days_before_month[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static time_t get_epoch_rtc(void) {
	int s, m, h, d, mo, y;
	int s2, m2, h2, d2, mo2, y2;

	// read until two consecutive reads match perfectly
	// to prevent catching the RTC mid update (eg: 10:59:59 > 11:00:00)
	do {
		read_rtc(&s, &m, &h, &d, &mo, &y);
		read_rtc(&s2, &m2, &h2, &d2, &mo2, &y2);
	} while (s != s2 || m != m2 || h != h2 || d != d2 || mo != mo2 || y != y2);

	time_t year = y + 2000;
	time_t total_days = (year - 1970) * 365;
	total_days += (year - 1969) / 4;
	total_days += days_before_month[mo - 1];

	if (mo > 2 && (year % 4 == 0)) {
		total_days++;
	}

	total_days += (d - 1);

	return (total_days * 86400) + (h * 3600) + (m * 60) + s;
}

// adjust RTC using HPET
void time_sync(void) {
	int start_sec, m, h, d, mo, y;
	read_rtc(&start_sec, &m, &h, &d, &mo, &y);
	int current_sec = start_sec;

	while(current_sec == start_sec) {
		__asm__ volatile("pause");
		read_rtc(&current_sec, &m, &h, &d, &mo, &y);
	}

	boot_hpet_ticks = hpet_get_ticks();
	boot_epoch = get_epoch_rtc();

	dev_puts(&uart16550_dev, "[ TIME ] Time synced\n");
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

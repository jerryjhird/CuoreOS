#include "_time.h"
#include <stdint.h>
#include "drivers/RTC.h"

static const int days_before_month[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

time_t get_epoch() {
	int s, m, h, d, mo, y;

	read_rtc(&s, &m, &h, &d, &mo, &y);

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

#pragma once

#include <stdint.h>

typedef struct {
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint32_t year;
	uint8_t weekday;
} rtc_time_t;

void read_rtc(rtc_time_t *time);

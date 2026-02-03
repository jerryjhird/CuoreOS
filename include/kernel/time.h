/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef TIME_H
#define TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;  // day of the month 1 - 31
    int tm_mon;   // months since january 0 - 11
    int tm_year;  // years since 1900
    int tm_wday;  // days since Sunday 0 - 6
    int tm_yday;  // days since january 1st 0 - 365
    int tm_isdst; // daylight saving
};

// src/other/x86.c
void sleep_ms(uint64_t ms, uint64_t cpu_hz);

uint32_t tm_to_epoch(const struct tm *tm);
struct tm gettm(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_H
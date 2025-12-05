#ifndef TIME_H
#define TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month;
    uint16_t year;
} datetime_st;

// src/other/x86.c
void sleep_ms(uint64_t ms, uint64_t cpu_hz);

uint32_t dttepoch(datetime_st dt);
datetime_st getdatetime(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_H
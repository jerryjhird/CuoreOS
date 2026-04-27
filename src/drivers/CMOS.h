#pragma once

#include <stdint.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

void cmos_write(uint8_t reg, uint8_t value);
uint8_t cmos_read(uint8_t reg);

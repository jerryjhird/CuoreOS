#pragma once

#include <stdint.h>

#define HPET_REG_CAPS 0x00
#define HPET_REG_CONFIG 0x10
#define HPET_REG_MAIN_COUNTER 0xF0

void hpet_init(void);
uint64_t hpet_get_nanos(void);
uint64_t hpet_get_ms(void);
uint32_t hpet_femto_per_tick(void);
uint64_t hpet_get_ticks(void);

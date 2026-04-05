#pragma once

#include <stdint.h>
#include <stdbool.h>

#define THERM_STATUS_CRITICAL (1 << 0)
#define THERM_STATUS_PROCHOT  (1 << 1)

typedef struct {
	uint32_t temp_c;
	bool is_critical;
	bool is_throttling;
} cpu_thermal_info_t;

cpu_thermal_info_t thermal_read(void);
bool does_cpu_support_dts(void);

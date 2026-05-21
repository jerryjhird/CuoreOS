#pragma once

#include <stdint.h>

#define MSR_IA32_APIC_BASE 0x1B
#define MSR_IA32_THERM_STATUS 0x19C
#define MSR_IA32_TEMPERATURE_TARGET 0x1A2
#define MSR_GS_BASE 0xC0000101

uint64_t read_msr(uint32_t msr);

#define WRITE_MSR(msr, val) do { \
	uint64_t __val = (uint64_t)(val); \
	__asm__ volatile ( \
		"wrmsr" \
		: \
		: "c"((uint32_t)(msr)), \
		  "a"((uint32_t)(__val & 0xFFFFFFFF)), \
		  "d"((uint32_t)(__val >> 32)) \
		: "memory" \
	); \
} while(0)

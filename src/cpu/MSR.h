#pragma once

#include <stdint.h>

#define MSR_IA32_APIC_BASE 0x1B
#define MSR_IA32_THERM_STATUS 0x19C
#define MSR_IA32_TEMPERATURE_TARGET 0x1A2
#define MSR_GS_BASE 0xC0000101

uint64_t read_msr(uint32_t msr);
void write_msr(uint32_t msr, uint64_t value);

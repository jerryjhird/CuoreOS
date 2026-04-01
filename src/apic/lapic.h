#pragma once

#include <stdint.h>

#define LAPIC_REG_TPR 0x0080
#define LAPIC_REG_EOI 0x00B0
#define LAPIC_REG_SPURIOUS 0x00F0
#define LAPIC_REG_LVT_TIMER	0x0320
#define LAPIC_REG_TIMER_INIT 0x0380
#define LAPIC_REG_TIMER_DIV	0x03E0
#define MSR_IA32_APIC_BASE 0x1B

void lapic_init(uintptr_t base_addr);
void lapic_eoi(void);

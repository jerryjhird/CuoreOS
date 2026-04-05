#pragma once

#include <stdint.h>

#define LAPIC_REG_ID 0x20
#define LAPIC_REG_TPR 0x0080
#define LAPIC_REG_EOI 0x00B0
#define LAPIC_REG_SPURIOUS 0x00F0
#define LAPIC_REG_LVT_TIMER 0x0320
#define LAPIC_REG_TIMER_INIT 0x0380
#define LAPIC_REG_TIMER_DIV 0x03E0
#define MSR_IA32_APIC_BASE 0x1B
#define MSR_IA32_THERM_STATUS 0x19C
#define MSR_IA32_TEMPERATURE_TARGET 0x1A2
#define LAPIC_REG_ICR_LOW 0x300
#define LAPIC_REG_ICR_HIGH 0x310
#define ICR_FIXED 0x000
#define ICR_INIT 0x500
#define ICR_STARTUP 0x600
#define ICR_PHYSICAL 0x000
#define ICR_LEVEL_ASSERT 0x4000
#define ICR_LEVEL_DEASSERT 0x000
#define ICR_SEND_PENDING 0x1000
#define CPUID_FEAT_ECX_RDRAND (1 << 30)
#define CPUID_FEAT_EBX_RDSEED (1 << 18)

void lapic_init(uintptr_t base_addr);

uint32_t lapic_get_id(void);
void lapic_send_ipi(uint8_t target_lapic_id, uint8_t vector);
void lapic_send_init(uint8_t target_lapic_id);
void lapic_send_sipi(uint8_t target_lapic_id, uintptr_t entry_point);

void lapic_eoi(void);

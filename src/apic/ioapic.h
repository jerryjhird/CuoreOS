#pragma once

#include <stdint.h>

#define IOAPIC_DELIVERY_FIXED  0x000
#define IOAPIC_POLARITY_LOW    (1 << 13)
#define IOAPIC_POLARITY_HIGH   (0 << 13)
#define IOAPIC_TRIGGER_LEVEL   (1 << 15)
#define IOAPIC_TRIGGER_EDGE    (0 << 15)

void ioapic_init(uintptr_t base_addr);
void ioapic_map_irq(uint8_t irq_pin, uint8_t vector, uint8_t cpu_apic_id, uint32_t flags);
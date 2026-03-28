#pragma once

#include <stdint.h>

void ioapic_init(uintptr_t base_addr);
void ioapic_map_irq(uint8_t irq_pin, uint8_t vector, uint8_t cpu_apic_id);
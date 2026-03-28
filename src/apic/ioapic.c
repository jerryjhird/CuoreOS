#include <stdint.h>
#include <limine.h>
#include "kstate.h"

#define IOAPIC_REG_SEL  0x00
#define IOAPIC_REG_WIN  0x10

static uintptr_t ioapic_virt_base = 0;

static void ioapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_SEL) = reg;
    *(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_WIN) = value;
}

void ioapic_map_irq(uint8_t irq_pin, uint8_t vector, uint8_t cpu_apic_id) {
    uint32_t low_index = 0x10 + (irq_pin * 2);

    ioapic_write(low_index + 1, (uint32_t)cpu_apic_id << 24);
    ioapic_write(low_index, (uint32_t)vector);
}

void ioapic_init(uintptr_t base_addr) {
    if (base_addr == 0) {
        panic("IOAPIC", "Received null base address!");
    }
    
    ioapic_virt_base = base_addr;
}
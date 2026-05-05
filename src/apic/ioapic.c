#include <stdint.h>
#include <limine.h>
#include "ioapic.h"
#include "logbuf.h"
#include "madt.h"

#define IOAPIC_REG_SEL  0x00
#define IOAPIC_REG_WIN  0x10

static uintptr_t ioapic_virt_base = 0;
static uint8_t pin_to_vector[24] = {0};
static uint8_t next_free_vector = 40;

static void ioapic_write(uint32_t reg, uint32_t value) {
	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_SEL) = reg;
	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_WIN) = value;
}

// not used yet
// static uint32_t ioapic_read(uint32_t reg) {
// 	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_SEL) = reg;
// 	return *(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_WIN);
// }

void ioapic_map_irq(uint8_t irq_pin, uint8_t vector, uint8_t cpu_apic_id, uint32_t flags) {
	uint32_t low_index = 0x10 + (irq_pin * 2);
	ioapic_write(low_index + 1, (uint32_t)cpu_apic_id << 24);
	uint32_t low_entry = (uint32_t)vector | flags;
	ioapic_write(low_index, low_entry);

	#ifdef DEBUG
		logbuf_printf("[IOAPIC] mapped pin: %u, vector: %#02x, flags: %#08x\n", (unsigned int)irq_pin, (unsigned int)vector, (unsigned int)flags);
	#endif
}

uint8_t ioapic_map_irq_to_free_vector(uint8_t irq_pin, uint8_t cpu_apic_id, uint32_t flags) {
	if (irq_pin >= 24) return 0;

	if (pin_to_vector[irq_pin] != 0) {
		return pin_to_vector[irq_pin];
	}

	uint8_t vector = next_free_vector++;
	ioapic_map_irq(irq_pin, vector, cpu_apic_id, flags);
	pin_to_vector[irq_pin] = vector;

	return vector;
}

void ioapic_init(uintptr_t base_addr) {
	ioapic_virt_base = base_addr;

	uint8_t pin;
	uint32_t flags;

	// Timer
	madt_query_irq(0, &pin, &flags);
	ioapic_map_irq(pin, 32, 0, flags);

	// Keyboard
	// madt_query_irq(1, &pin, &flags);
	// ioapic_map_irq(pin, 33, 0, flags);

	// COM1
	madt_query_irq(4, &pin, &flags);
	ioapic_map_irq(pin, 36, 0, flags);
}

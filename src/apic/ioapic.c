#include <stdint.h>
#include <limine.h>
#include "kstate.h"
#include "ioapic.h"
#include "logbuf.h"
#include "madt.h"

#define IOAPIC_REG_SEL  0x00
#define IOAPIC_REG_WIN  0x10

static uintptr_t ioapic_virt_base = 0;

static void ioapic_write(uint32_t reg, uint32_t value) {
	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_SEL) = reg;
	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_WIN) = value;
}

static uint32_t ioapic_read(uint32_t reg) {
	*(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_SEL) = reg;
	return *(volatile uint32_t*)(ioapic_virt_base + IOAPIC_REG_WIN);
}

void ioapic_map_irq(uint8_t irq_pin, uint8_t vector, uint8_t cpu_apic_id, uint32_t flags) {
	uint32_t low_index = 0x10 + (irq_pin * 2);
	ioapic_write(low_index + 1, (uint32_t)cpu_apic_id << 24);
	uint32_t low_entry = (uint32_t)vector | flags;
	ioapic_write(low_index, low_entry);

	#ifdef DEBUG
		logbuf_write("[IOAPIC] mapped pin: ");
		logbuf_putint(irq_pin);
		logbuf_write(", vector:");
		logbuf_putint(vector);
		logbuf_write(", flags:");
		logbuf_putint(flags);
		logbuf_write("\n");
	#endif
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

#include "lapic.h"

static uintptr_t lapic_base = 0;

void lapic_write(uint32_t reg, uint32_t data) {
	*(volatile uint32_t*)(lapic_base + reg) = data;
}

void lapic_init(uintptr_t base_addr) {
	lapic_base = base_addr;

	// ensure APIC is enabled in MSR
	uint32_t low, high;
	__asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_IA32_APIC_BASE));
	if (!(low & (1 << 11))) {
		low |= (1 << 11);
		__asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(MSR_IA32_APIC_BASE));
	}

	lapic_write(LAPIC_REG_TPR, 0);
	lapic_write(LAPIC_REG_SPURIOUS, 0x1FF);
	lapic_write(LAPIC_REG_TIMER_DIV, 0x3);
	lapic_write(LAPIC_REG_LVT_TIMER, 32 | (1 << 17));
	lapic_write(LAPIC_REG_TIMER_INIT, 0x400000);
}

void lapic_eoi(void) {
	if (lapic_base) {
		lapic_write(LAPIC_REG_EOI, 0);
	}
}

#include "lapic.h"
#include "kstate.h"

#define LAPIC_REG_TPR		   0x0080
#define LAPIC_REG_EOI		   0x00B0
#define LAPIC_REG_SPURIOUS	  0x00F0
#define LAPIC_REG_LVT_TIMER	 0x0320
#define LAPIC_REG_TIMER_INIT	0x0380
#define LAPIC_REG_TIMER_DIV	 0x03E0
#define MSR_IA32_APIC_BASE	  0x1B

static uintptr_t lapic_base = 0;

static void lapic_write(uint32_t reg, uint32_t data) {
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
	lapic_write(LAPIC_REG_TIMER_INIT, 0x1000000);
}

void lapic_eoi(void) {
	if (lapic_base) {
		lapic_write(LAPIC_REG_EOI, 0);
	}
}

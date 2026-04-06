#include "lapic.h"
#include "cpu/MSR.h"

static uintptr_t lapic_base = 0;

static void lapic_write(uint32_t reg, uint32_t data) {
	*(volatile uint32_t*)(lapic_base + reg) = data;
}

static uint32_t lapic_read(uint32_t reg) {
	return *(volatile uint32_t*)(lapic_base + reg);
}

static void lapic_wait_for_delivery(void) {
	while (lapic_read(LAPIC_REG_ICR_LOW) & ICR_SEND_PENDING) {
		__asm__ volatile ("pause");
	}
}

void lapic_send_ipi(uint8_t target_lapic_id, uint8_t vector) {
	lapic_wait_for_delivery();
	lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);
	lapic_write(LAPIC_REG_ICR_LOW, ICR_PHYSICAL | ICR_LEVEL_ASSERT | ICR_FIXED | vector);
}

void lapic_send_init(uint8_t target_lapic_id) {
	lapic_wait_for_delivery();
	lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);

	lapic_write(LAPIC_REG_ICR_LOW, ICR_INIT | ICR_PHYSICAL | ICR_LEVEL_ASSERT);
	lapic_wait_for_delivery();
	lapic_write(LAPIC_REG_ICR_LOW, ICR_INIT | ICR_PHYSICAL | ICR_LEVEL_DEASSERT);
}

void lapic_send_sipi(uint8_t target_lapic_id, uintptr_t entry_point) {
	lapic_wait_for_delivery();
	lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);

	uint8_t vector = (uint8_t)((entry_point >> 12) & 0xFF);

	lapic_write(LAPIC_REG_ICR_LOW, ICR_STARTUP | ICR_PHYSICAL | vector);
}

uint32_t lapic_get_id(void) {
	return lapic_read(LAPIC_REG_ID) >> 24;
}

void lapic_init(uintptr_t base_addr) {
	lapic_base = base_addr;

	uint64_t apic_base = read_msr(MSR_IA32_APIC_BASE);

	if (!(apic_base & (1ULL << 11))) {
		apic_base |= (1ULL << 11);
		write_msr(MSR_IA32_APIC_BASE, apic_base);
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

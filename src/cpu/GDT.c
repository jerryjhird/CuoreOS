#include <stdint.h>

struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
	gdt[i].limit_low = limit & 0xFFFF;
	gdt[i].base_low = base & 0xFFFF;
	gdt[i].base_mid = (base >> 16) & 0xFF;
	gdt[i].access = access;
	gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
	gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void) {
	gp.limit = sizeof(gdt) - 1;
	gp.base = (uint64_t)&gdt;

	gdt_set(0, 0, 0, 0, 0);
	gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); // Kernel Code
	gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xA0); // Kernel Data

	__asm__ volatile("lgdt %0" : : "m"(gp) : "memory");

	__asm__ volatile(
		"mov $0x10, %%ax\n\t"
		"mov %%ax, %%ds\n\t"
		"mov %%ax, %%es\n\t"
		"mov %%ax, %%ss\n\t"
		"pushq $0x08\n\t"
		"leaq 1f(%%rip), %%rax\n\t"
		"pushq %%rax\n\t"
		"lretq\n\t"
		"1:\n\t"
		: : : "rax", "memory");
}
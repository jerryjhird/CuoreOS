#ifndef X86_H
#define X86_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint8_t cmos_read(uint8_t reg);
void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx);

// helpers
uint8_t bcdtbin(uint8_t val); // bcd to binary
uint64_t rdtsc(void);

// global descriptor table

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void);

#ifdef __cplusplus
}
#endif


// interupt descriptor table

#define IDT_ENTRIES 256
#define NUM_GPRS 15

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;       // bits 0-2 = IST, rest zero
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);

#endif // X86_H


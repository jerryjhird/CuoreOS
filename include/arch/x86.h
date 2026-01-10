#ifndef X86_64_H
#define X86_64_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdio.h"

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx);
void halt(void);

// descriptor tables
// 1. global descriptor table

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

// 2. interupt descriptor table

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

// high level abstractions
char* cpu_brand(void);
void regtserial(uint64_t *regs); // dump registers to serial

// SSE
int sse_init(void);
uint32_t crc32c_hwhash(const char *s);

// stack
void swstack_jmp(void *new_sp, void (*entry)(void));

#endif // X86_64_H



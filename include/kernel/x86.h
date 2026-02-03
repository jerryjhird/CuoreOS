/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef X86_64_H
#define X86_64_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;  // stack to use when entering ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx);

uint64_t read_cr3(void);

void halt(void);

// high level abstractions
char* cpu_brand(void);
void regtserial(uint64_t *regs); // dump registers to serial

// SSE
int sse_init(void);

// stack
void swstack_jmp(void *new_sp, void (*entry)(void));

// userspace
void enter_userspace(uint64_t rip, uint64_t rsp);

#ifdef __cplusplus
}
#endif

#endif // X86_64_H



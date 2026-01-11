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

#include "stdint.h"
#include "stdio.h"

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx);
void halt(void);

// high level abstractions
char* cpu_brand(void);
void regtserial(uint64_t *regs); // dump registers to serial

// SSE
int sse_init(void);
uint32_t crc32c_hwhash(const char *s);

// stack
void swstack_jmp(void *new_sp, void (*entry)(void));

#ifdef __cplusplus
}
#endif

#endif // X86_64_H



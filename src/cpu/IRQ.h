#pragma once

#include <stdint.h>

struct trap_frame {
	uint64_t cr2, cr3, cr4; // control registers
	uint64_t ds, es, fs, gs; // segment registers

	// gp registers
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uint64_t error_code;
	uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

typedef struct trap_frame* (*irq_handler_t)(struct trap_frame *tf);

#include "cpu/smp/init.h"
void irq_install_handler(logical_coreid_t logical_id, uint8_t vector, irq_handler_t handler);

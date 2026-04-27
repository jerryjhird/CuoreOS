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

#define MAX_HANDLERS_PER_VECTOR 4

typedef struct {
	irq_handler_t handlers[MAX_HANDLERS_PER_VECTOR];
	uint8_t count;
} irq_vector_chain_t;

void irq_install_handler(uint32_t logical_id, uint8_t vector, irq_handler_t handler);

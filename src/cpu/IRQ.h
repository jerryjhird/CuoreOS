#pragma once

#include <stdint.h>

struct trap_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

typedef void (*irq_handler_t)(struct trap_frame *tf);
void irq_install_handler(uint8_t vector, irq_handler_t handler);


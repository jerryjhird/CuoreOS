#pragma once

#include <stdint.h>
#include "scheduler.h"
#include "cpu/smp/mailbox.h"
#include "IRQ.h"
#include "Config.h"

typedef enum { CPU_IDLE = 0, CPU_BUSY, CPU_HALTED, CPU_PANIC } cpu_status_t;
typedef struct coreinfo_t {
	struct coreinfo_t *self; // @ gs:0
	task_t *current_task; // @ gs:8
	uint32_t logical_id;
	uint32_t lapic_id;
	uint64_t ticks;
	volatile cpu_status_t status;
	mailbox_t mailbox;

	irq_vector_chain_t routines[256];
	uint64_t irq_stats[256];
} __attribute__((aligned(64))) coreinfo_t;

#define GET_CURRENT_CPU(target) __asm__ volatile ("mov %%gs:0, %0" : "=r"(target))
#define GET_CURRENT_TASK(target) __asm__ volatile ("mov %%gs:8, %0" : "=r"(target))
#define SET_CURRENT_TASK(bootstrap_task) __asm__ volatile ("mov %0, %%gs:8" : : "r"(bootstrap_task));

extern coreinfo_t *cpu_blocks[SMP_MAX_CORES];
extern uint32_t cpu_blocks_c;

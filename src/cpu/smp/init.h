#pragma once

#include <stdint.h>
#include "cpu/thermal.h"

typedef enum {
	CPU_IDLE = 0,
	CPU_BUSY,
	CPU_HALTED,
	CPU_PANIC
} cpu_status_t;

#define logical_coreid_t uint32_t // logical core id type

#include "cpu/IRQ.h"
#include "mailbox.h"

typedef struct cpu_control_block {
	struct cpu_control_block *self;
	logical_coreid_t logical_id;
	uint32_t lapic_id;
	uint64_t ticks;
	volatile cpu_status_t status;
	mailbox_t mailbox;

	bool dts_support;
	thermal_info_t thermal;

	irq_handler_t routines[256];
} __attribute__((aligned(64))) cpu_control_block_t;

extern cpu_control_block_t *logical_indexed_cpu_list[SMP_MAX_CORES];

#define GET_CURRENT_CPU(target) __asm__ volatile ("mov %%gs:0, %0" : "=r"(target))

void AP_kstartc(struct limine_mp_info *mp);
int get_idle_core(void);

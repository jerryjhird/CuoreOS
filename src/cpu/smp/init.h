#pragma once

#include <stdint.h>
#include "mailbox.h"
#include "cpu/thermal.h"
#include "cpu/IRQ.h"

typedef enum {
	CPU_IDLE = 0,
	CPU_BUSY,
	CPU_HALTED,
	CPU_PANIC
} cpu_status_t;

typedef struct {
	uint8_t lapic_id;
	uint64_t ticks;
	volatile cpu_status_t status;
	mailbox_t mailbox;
	bool dts_support;
	cpu_thermal_info_t thermal;

	irq_handler_t routines[SMP_MAX_CORES];
} cpu_control_block_t;

extern cpu_control_block_t cpus[SMP_MAX_CORES];

void AP_kstartc(struct limine_mp_info *mp);
int get_idle_core(void);

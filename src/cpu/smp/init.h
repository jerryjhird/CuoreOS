#pragma once

#include <stdint.h>
#include "mailbox.h"

typedef enum {
	CPU_IDLE = 0,
	CPU_BUSY,
	CPU_HALTED,
	CPU_PANIC
} cpu_status_t;

typedef struct {
	uint8_t lapic_id;
	volatile cpu_status_t status;
	mailbox_t mailbox;
	uint64_t current_thread_id;
} cpu_control_block_t;

extern cpu_control_block_t cpus[256];

void AP_kstartc(struct limine_mp_info *mp);
int get_idle_core(void);

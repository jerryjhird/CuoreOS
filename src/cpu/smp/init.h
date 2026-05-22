#pragma once

#include <stdint.h>
#include <stddef.h>

struct limine_mp_info;
extern volatile size_t cpu_online_count;

void AP_kstartc(struct limine_mp_info *mp);

struct trap_frame* AP_ipi_wakeup_irq(struct trap_frame *tf);
struct trap_frame* AP_clock_tick_irq(struct trap_frame *tf);

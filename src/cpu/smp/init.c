#include "logbuf.h"
#include "kstate.h"
#include "mem/mem.h"
#include "mem/heap.h"
#include "sync.h"
#include "mailbox.h"
#include "apic/lapic.h"
#include "cpu/GDT.h"
#include "cpu/IRQ.h"
#include "cpu/thermal.h"
#include "apic/madt.h"
#include <stdint.h>
#include "init.h"
#include "cpu/MSR.h"

static spinlock_t temp_spinlock = SPINLOCK_INIT;
cpu_control_block_t *logical_indexed_cpu_list[SMP_MAX_CORES];
volatile logical_coreid_t online_cpu_index = 0;

static struct trap_frame* ipi_wakeup_irq(struct trap_frame *tf) {
	   // (dispatcher will call lapic_eoi for us)
	   return tf;
}

static struct trap_frame* clock_tick_irq(struct trap_frame *tf) {
	cpu_control_block_t *my_cpu; GET_CURRENT_CPU(my_cpu);

	my_cpu->ticks++;

	if (my_cpu->ticks % 100 == 0 && my_cpu->dts_support) {
	   my_cpu->thermal = thermal_read();
	}

	return tf;
}

__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	uint64_t sp; __asm__ volatile("mov %%rsp, %0" : "=r"(sp));

	gdt_init();
	idt_init();

	lapic_init(madt_get_lapic_base() + hhdm_offset);

	static volatile int next_id = 1;
	logical_coreid_t logical_id = __atomic_fetch_add(&next_id, 1, __ATOMIC_SEQ_CST);

	SPIN_LOCK(&temp_spinlock);
	cpu_control_block_t *my_cpu = zalloc(sizeof(cpu_control_block_t));
	SPIN_UNLOCK(&temp_spinlock);

	logical_indexed_cpu_list[logical_id] = my_cpu;
	my_cpu->self = my_cpu;

	__asm__ volatile ("wrmsr" : : "c"(MSR_GS_BASE), "a"((uint32_t)(uint64_t)my_cpu), "d"((uint32_t)((uint64_t)my_cpu >> 32))); 	// store a pointer to this CPU block in GS base

	my_cpu->logical_id = logical_id;
	my_cpu->lapic_id = mp->lapic_id;;
	my_cpu->ticks = 1;
	my_cpu->dts_support = does_cpu_support_dts();
	if (my_cpu->dts_support) {my_cpu->thermal = thermal_read();}

	irq_install_handler(logical_id, 40, ipi_wakeup_irq);
	// irq_install_handler(logical_id, 32, clock_tick_irq);

	SPIN_LOCK(&temp_spinlock);
	logbuf_write("[ SMP  ] Core ");
	logbuf_putint(logical_id);
	logbuf_write(" Initialized = lapic: ");
	logbuf_putint(mp->lapic_id);
	logbuf_write(" | sp: ");
	logbuf_puthex64(sp);
	logbuf_write("\n");
	SPIN_UNLOCK(&temp_spinlock);

	__atomic_fetch_add(&online_cpu_index, 1, __ATOMIC_SEQ_CST);

	while (1) {
		wait_for_work:
		my_cpu->status = CPU_IDLE;
		__asm__ volatile("sti; hlt");

		if (__atomic_load_n(&my_cpu->mailbox.pending, __ATOMIC_ACQUIRE)) {
			my_cpu->status = CPU_BUSY;

			if (my_cpu->mailbox.func) {
				my_cpu->mailbox.func();
			}

			__atomic_store_n(&my_cpu->mailbox.pending, false, __ATOMIC_RELEASE);
			goto wait_for_work;
		}
	}
}

// find a free cpu
int get_idle_core(void) {
	cpu_control_block_t *self; GET_CURRENT_CPU(self);
	uint64_t count = __atomic_load_n(&online_cpu_index, __ATOMIC_ACQUIRE);

	for (uint64_t i = 0; i < count; i++) {
		cpu_control_block_t *cpu = logical_indexed_cpu_list[i];
		if (cpu == self) continue;

		if (cpu->status == CPU_IDLE && !cpu->mailbox.pending) {
			return i;
		}
	}
	return -1;
}

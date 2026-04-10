#include "logbuf.h"
#include "kstate.h"
#include "sync.h"
#include "mailbox.h"
#include "apic/lapic.h"
#include "cpu/GDT.h"
#include "cpu/IRQ.h"
#include "cpu/thermal.h"
#include "apic/madt.h"
#include <stdint.h>
#include "init.h"

static spinlock_t temp_spinlock = SPINLOCK_INIT;
cpu_control_block_t cpus[SMP_MAX_CORES];
cpu_control_block_t* online_cpus[SMP_MAX_CORES];
volatile uint64_t online_cpu_index = 0;

static struct trap_frame* ipi_wakeup_irq(struct trap_frame *tf) {
	   // (dispatcher will call lapic_eoi for us)
	   return tf;
}

static struct trap_frame* clock_tick_irq(struct trap_frame *tf) {
	uint8_t my_id = (uint8_t)lapic_get_id();
	cpu_control_block_t *my_cpu = &cpus[my_id];

	my_cpu->ticks++;

	if (my_cpu->ticks % 100 == 0 && my_cpu->dts_support) {
	   my_cpu->thermal = thermal_read();
	}

	return tf;
}

__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	gdt_init();
	idt_init();

	lapic_init(madt_get_lapic_base() + hhdm_offset);
	uint8_t my_id = (uint8_t)mp->lapic_id;

	irq_install_handler(my_id, 40, ipi_wakeup_irq);
	irq_install_handler(my_id, 32, clock_tick_irq);

	cpu_control_block_t *my_cpu = &cpus[my_id];
	my_cpu->lapic_id = my_id;
	my_cpu->ticks = 1;
	my_cpu->dts_support = does_cpu_support_dts();

	SPIN_LOCK(&temp_spinlock);
	logbuf_write("+ [ SMP  ] CPU ");
	logbuf_puthex(my_id);
	logbuf_write(" Online\n");
	SPIN_UNLOCK(&temp_spinlock);

	int idx = __atomic_fetch_add(&online_cpu_index, 1, __ATOMIC_SEQ_CST);
	online_cpus[idx] = my_cpu;

	while (1) {
		wait_for_work:
		my_cpu->status = CPU_IDLE;
		__asm__ volatile("sti; hlt");

		if (__atomic_load_n(&my_cpu->mailbox.pending, __ATOMIC_ACQUIRE)) {
			my_cpu->status = CPU_BUSY;

			if (my_cpu->mailbox.func) {
				my_cpu->mailbox.func(my_cpu->mailbox.data);
			}

			__atomic_store_n(&my_cpu->mailbox.pending, false, __ATOMIC_RELEASE);
			goto wait_for_work;
		}
	}
}

// find a free cpu
int get_idle_core(void) {
	uint8_t self = (uint8_t)lapic_get_id();
	uint64_t count = __atomic_load_n(&online_cpu_index, __ATOMIC_ACQUIRE);

	for (uint64_t i = 0; i < count; i++) {
		cpu_control_block_t* cpu = online_cpus[i];

		if (cpu == NULL) continue;
		if (cpu->lapic_id == self) continue;

		if (cpu->status == CPU_IDLE && !cpu->mailbox.pending) {
			return (int)cpu->lapic_id;
		}
	}
	return -1;
}

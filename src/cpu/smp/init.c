#include "init.h"

#include "logbuf.h"
#include "mem/heap.h"
#include "sync.h"
#include "mailbox.h"
#include "cpu/apic/lapic.h"
#include "cpu/GDT.h"
#include "cpu/IRQ.h"
#include <stdint.h>
#include "cpu/MSR.h"
#include "cpu/coreinfo.h"

#include <limine.h>

static spinlock_t AP_init_spinlock = SPINLOCK_INIT;
volatile size_t cpu_online_count = 0;

struct trap_frame* AP_ipi_wakeup_irq(struct trap_frame *tf) {
	   // (dispatcher will call lapic_eoi for us)
	   return tf;
}

struct trap_frame* AP_clock_tick_irq(struct trap_frame *tf) {
	coreinfo_t *my_cpu; GET_CURRENT_CPU(my_cpu);
	my_cpu->ticks++;
	return tf;
}

// NOTE: swap out limine_mp_info with a custom struct eventually
__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	gdt_init();
	idt_init();
	lapic_init(100);

	SPIN_LOCK(&AP_init_spinlock);

	uint32_t my_index = cpu_blocks_c++;
	coreinfo_t *my_cpu = zalloc(sizeof(coreinfo_t));

	my_cpu->self = my_cpu;
	my_cpu->logical_id = my_index;
	my_cpu->lapic_id = mp->lapic_id;
	my_cpu->ticks = 1;
	my_cpu->status = CPU_IDLE;
	my_cpu->mailbox.pending = false;
	my_cpu->mailbox.func = NULL;

	cpu_blocks[my_index] = my_cpu;
	WRITE_MSR(MSR_GS_BASE, (uint64_t)my_cpu);

	logbuf_ok("[ SMP  ] Core %u Initialized | lapic: %u\n", (unsigned int)my_index, (unsigned int)mp->lapic_id);

	__atomic_fetch_add(&cpu_online_count, 1, __ATOMIC_SEQ_CST);

	SPIN_UNLOCK(&AP_init_spinlock);

	while (1) {
		my_cpu->status = CPU_IDLE;
		__asm__ volatile("sti; hlt");

		if (__atomic_load_n(&my_cpu->mailbox.pending, __ATOMIC_ACQUIRE)) {
			my_cpu->status = CPU_BUSY;
			if (my_cpu->mailbox.func) my_cpu->mailbox.func();
			__atomic_store_n(&my_cpu->mailbox.pending, false, __ATOMIC_RELEASE);
		}
	}
}

// find a free cpu
int get_idle_core(void) {
	coreinfo_t *self;
	GET_CURRENT_CPU(self);

	uint64_t count = __atomic_load_n(&cpu_blocks_c, __ATOMIC_ACQUIRE);

	for (uint64_t i = 0; i < count; i++) {
		coreinfo_t *cpu = cpu_blocks[i];

		if (!cpu || cpu == self) continue;

		if (__atomic_load_n(&cpu->status, __ATOMIC_ACQUIRE) == CPU_IDLE &&
			!__atomic_load_n(&cpu->mailbox.pending, __ATOMIC_ACQUIRE)) {
			return (int)i;
		}
	}
	return -1;
}

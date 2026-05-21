#include "logbuf.h"
#include "kstate.h"
#include "mem/mem.h"
#include "mem/heap.h"
#include "sync.h"
#include "mailbox.h"
#include "apic/lapic.h"
#include "cpu/GDT.h"
#include "cpu/IRQ.h"
#include "apic/madt.h"
#include <stdint.h>
#include "init.h"
#include "cpu/MSR.h"

static spinlock_t temp_spinlock = SPINLOCK_INIT;

static struct trap_frame* ipi_wakeup_irq(struct trap_frame *tf) {
	   // (dispatcher will call lapic_eoi for us)
	   return tf;
}

static struct trap_frame* clock_tick_irq(struct trap_frame *tf) {
	kernel_cpu_dev_t *my_cpu; GET_CURRENT_CPU(my_cpu);
	my_cpu->ticks++;
	return tf;
}

__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	uint64_t sp; __asm__ volatile("mov %%rsp, %0" : "=r"(sp));

	gdt_init();
	idt_init();

	lapic_init(madt_get_lapic_base() + hhdm_offset);

	static volatile int next_id = 1;
	uint32_t logical_id = __atomic_fetch_add(&next_id, 1, __ATOMIC_SEQ_CST);

	SPIN_LOCK(&temp_spinlock);
	kernel_cpu_dev_t *my_cpu = zalloc(sizeof(kernel_cpu_dev_t));
	SPIN_UNLOCK(&temp_spinlock);

	cpu_devices[logical_id] = my_cpu;
	my_cpu->self = my_cpu;

	WRITE_MSR(MSR_GS_BASE, (uint64_t)my_cpu);

	my_cpu->logical_id = logical_id;
	my_cpu->lapic_id = mp->lapic_id;;
	my_cpu->ticks = 1;

	irq_install_handler(logical_id, 40, ipi_wakeup_irq);
	irq_install_handler(logical_id, 32, clock_tick_irq);

	SPIN_LOCK(&temp_spinlock);
		logbuf_printf("[ SMP  ] Core %u Initialized = lapic: %u | sp: %p\n", (unsigned int)logical_id, (unsigned int)mp->lapic_id, (void*)sp);
	SPIN_UNLOCK(&temp_spinlock);

	__atomic_fetch_add(&cpu_devices_c, 1, __ATOMIC_SEQ_CST);

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
	kernel_cpu_dev_t *self;
	GET_CURRENT_CPU(self);
	uint64_t count = __atomic_load_n(&cpu_devices_c, __ATOMIC_ACQUIRE);

	for (uint64_t i = 0; i < count; i++) {
		kernel_cpu_dev_t *cpu = cpu_devices[i];

		if (!cpu || cpu == self) continue;

		if (cpu->status == CPU_IDLE && !cpu->mailbox.pending) {
			return i;
		}
	}
	return -1;
}

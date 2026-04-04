#include "devices.h"
#include "logbuf.h"
#include "kstate.h"
#include "sync.h"
#include "mailbox.h"
#include "apic/lapic.h"
#include "cpu/GDT.h"
#include "cpu/IRQ.h"
#include "apic/madt.h"
#include <stdint.h>
#include "init.h"

static spinlock_t temp_spinlock = SPINLOCK_INIT;
cpu_control_block_t cpus[256];

struct trap_frame* ipi_wakeup_irq(struct trap_frame *tf) {
	   // (dispatcher will call lapic_eoi for us)
	   return tf;
}

__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	gdt_init();
	idt_init();

	lapic_init(madt_get_lapic_base() + hhdm_offset);
	uint8_t my_id = (uint8_t)mp->lapic_id;

	irq_install_handler(0x40, ipi_wakeup_irq);

	cpu_control_block_t *my_cpu = &cpus[my_id];
	my_cpu->lapic_id = my_id;

	SPIN_LOCK(&temp_spinlock);
	logbuf_write("+ [ SMP  ] CPU ");
	logbuf_puthex(my_id);
	logbuf_write(" Online\n");
	SPIN_UNLOCK(&temp_spinlock);

	__atomic_fetch_add(&online_cpu_count, 1, __ATOMIC_SEQ_CST);

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

	for (uint8_t i = 0; i < online_cpu_count; i++) {
		if (i == 0) continue;
		if (i == self) continue;

		if (cpus[i].status == CPU_IDLE && !cpus[i].mailbox.pending) {
			return (int)i;
		}
	}
	return -1; // all cores are busy
}

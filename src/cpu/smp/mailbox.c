#include "mailbox.h"
#include "apic/lapic.h"
#include <stdint.h>
#include "kstate.h"
#include "init.h"

// send to a specific cpu
void mailbox_send(uint8_t cpu_id, mailbox_func_t func, void *data) {
	cpu_control_block_t *target = &cpus[cpu_id];

	while (__atomic_load_n(&target->mailbox.pending, __ATOMIC_ACQUIRE)) {
		__asm__ volatile("pause");
	}

	target->mailbox.func = func;
	target->mailbox.data = data;

	__atomic_store_n(&target->mailbox.pending, true, __ATOMIC_RELEASE);
	lapic_send_ipi(cpu_id, 0x40);
}

// send to everyone
void mailman_send(mailbox_func_t func, void *data) {
	for (uint8_t i = 0; i < online_cpu_count; i++) {
		if (i == (uint8_t)lapic_get_id()) continue;
		mailbox_send(i, func, data);
	}
	func(data);
}

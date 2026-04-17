#include "mailbox.h"
#include "apic/lapic.h"
#include <stdint.h>
#include "kstate.h"
#include "init.h"

// send to a specific cpu
void mailbox_send(logical_coreid_t logical_id, mailbox_func_t func, void *data) {
	cpu_control_block_t *target = logical_indexed_cpu_list[logical_id];

	while (__atomic_load_n(&target->mailbox.pending, __ATOMIC_ACQUIRE)) {
		__asm__ volatile("pause");
	}

	target->mailbox.func = func;
	target->mailbox.data = data;

	__atomic_store_n(&target->mailbox.pending, true, __ATOMIC_RELEASE);

	lapic_send_ipi(target->lapic_id, 40);
}

// send to everyone
void mailman_send(mailbox_func_t func, void *data) {
	cpu_control_block_t *self; GET_CURRENT_CPU(self);

	for (logical_coreid_t i = 0; i < online_cpu_index; i++) {
		if (i == self->logical_id) continue;

		mailbox_send(i, func, data);
	}

	func();
}

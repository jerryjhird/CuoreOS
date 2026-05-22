#include "mailbox.h"

#include "apic/lapic.h"
#include <stdint.h>
#include "init.h"
#include "abs.h"
#include "panic.h"
#include "cpu/coreinfo.h"

// send to a specific cpu
void mailbox_send(uint32_t logical_id, mailbox_func_t func, void *data) {
	coreinfo_t *target = cpu_blocks[logical_id];

	if (UNLIKELY(!target)) {
		panic("MAILBOX", "passed logical_id not found\n");
	}

	while (__atomic_load_n(&target->mailbox.pending, __ATOMIC_ACQUIRE)) {
		__asm__ volatile("pause");
	}

	target->mailbox.func = func;
	target->mailbox.data = data;

	__atomic_store_n(&target->mailbox.pending, true, __ATOMIC_RELEASE);

	lapic_send_ipi(target->lapic_id, 40);
}

// send to any idle cpu
bool mailbox_send_fc(mailbox_func_t func, void *data) {
	int target = get_idle_core();
	if (target == -1) { return false; }

	mailbox_send(target, func, data);
	return true;
}

// send to everyone
void mailman_send(mailbox_func_t func, void *data) {
	coreinfo_t *self; GET_CURRENT_CPU(self);

	for (uint32_t i = 0; i < cpu_online_count; i++) {
		if (i == self->logical_id) continue;

		mailbox_send(i, func, data);
	}

	func();
}

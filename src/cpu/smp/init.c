#include "devices.h"
#include "logbuf.h"
#include "kstate.h"
#include "sync.h"

static spinlock_t temp_spinlock = SPINLOCK_INIT;

__attribute__((noinline))
void AP_kstartc(struct limine_mp_info *mp) {
	SPIN_LOCK(&temp_spinlock)
	logbuf_write("+ [ SMP  ] CPU ");
	logbuf_puthex(mp->lapic_id);
	logbuf_write(" Initialized\n");
	SPIN_UNLOCK(&temp_spinlock);

	__atomic_fetch_add(&online_cpu_count, 1, __ATOMIC_SEQ_CST);
	while (1) { __asm__ volatile("pause; hlt"); }
}

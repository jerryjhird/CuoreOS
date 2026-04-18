#pragma once

#include <stdatomic.h>

#define SPINLOCK_INIT ATOMIC_FLAG_INIT

typedef atomic_flag spinlock_t;

#define SPIN_LOCK(lock) while (atomic_flag_test_and_set(lock)) { __asm__ volatile ("pause"); }
#define SPIN_UNLOCK(lock) atomic_flag_clear(lock)

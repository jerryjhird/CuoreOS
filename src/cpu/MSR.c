#include "MSR.h"

#include <stdint.h>

uint64_t read_msr(uint32_t msr) {
	uint32_t low, high;
	__asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return ((uint64_t)high << 32) | low;
}

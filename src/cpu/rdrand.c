#include "rdrand.h"
#include "cpu/cpuid.h"
#include <stdint.h>
#include <stdbool.h>

bool rdrand_supported(void) {
	uint32_t eax, ebx, ecx, edx;
	CPUID(1, 0, eax, ebx, ecx, edx);
	return (ecx & CPUID_RDRAND_SUPPORTED) != 0;
}

bool rdseed_supported(void) {
	uint32_t eax, ebx, ecx, edx;
	CPUID(7, 0, eax, ebx, ecx, edx);
	return (ebx & CPUID_RDSEED_SUPPORTED) != 0;
}

bool rdrand64(uint64_t *output) {
	uint8_t ok;
	__asm__ volatile (
		"rdrand %0; setc %1"
		: "=r" (*output), "=qm" (ok)
		:: "cc"
	);
	return ok == 1;
}

bool rdseed64(uint64_t *output) {
	uint8_t ok;
	__asm__ volatile (
		"rdseed %0; setc %1"
		: "=r" (*output), "=qm" (ok)
		:: "cc"
	);
	return ok == 1;
}

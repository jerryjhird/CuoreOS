#pragma once

#define CPUID_DTS_SUPPORTED (1 << 0)
#define CPUID_ARAT_SUPPORTED (1 << 2) // always running APIC timer
#define CPUID_RDSEED_SUPPORTED (1 << 18)
#define CPUID_RDRAND_SUPPORTED (1 << 30)

#define CPUID_LEAF_THERMAL_POWER 6

#define CPUID(leaf, subleaf, a, b, c, d) \
	__asm__ volatile ("cpuid" \
					  : "=a"(a), "=b"(b), "=c"(c), "=d"(d) \
					  : "a"(leaf), "c"(subleaf))

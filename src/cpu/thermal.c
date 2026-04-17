#include "thermal.h"
#include "MSR.h"
#include "cpu/cpuid.h"

thermal_info_t thermal_read(void) {
	thermal_info_t info = {0};

	uint64_t target_reg = read_msr(MSR_IA32_TEMPERATURE_TARGET);
	uint32_t tj_max = (target_reg >> 16) & 0xFF;

	uint64_t status_reg = read_msr(MSR_IA32_THERM_STATUS);
	uint32_t delta = (status_reg >> 16) & 0x7F;

	info.temp_c = tj_max - delta;
	info.is_critical = (status_reg & THERM_STATUS_CRITICAL) ? true : false;
	info.is_throttling = (status_reg & THERM_STATUS_PROCHOT) ? true : false;

	return info;
}

bool does_cpu_support_dts(void) {
	uint32_t eax, ebx, ecx, edx;

	CPUID(CPUID_LEAF_THERMAL_POWER, 0, eax, ebx, ecx, edx);

	if (!(eax & CPUID_DTS_SUPPORTED)) {
		// CPU doesnt support DTS
		return false;
	}

	if (eax & CPUID_ARAT_SUPPORTED) {
		// APIC timer is reliable in all power states
	}

	return true;
}

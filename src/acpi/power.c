#include "power.h"

#include "acpi.h"
#include "fadt.h"
#include "logbuf.h"
#include "mem/mem.h"
#include "devices.h"
#include "cpu/io.h"
#include "mem/heap.h"
#include "builtinabs.h"

extern acpi_fadt_t* fadt;

static bool acpi_shutdown(kernel_power_dev_t* dev) {
	(void)dev;
	uintptr_t cnt_blk = fadt_get_pm1a_cnt();
	bool is_mmio = fadt_is_pm1a_mmio();
	uint16_t s5_val = fadt_get_s5_types();

	if (cnt_blk == 0) return false;

	uint16_t cmd = s5_val | (1 << 13);

	if (is_mmio) {
		*(volatile uint16_t*)(cnt_blk - 4 + hhdm_offset) = 0xFFFF;
		*(volatile uint16_t*)(cnt_blk + hhdm_offset) = cmd;
	} else {
		outw((uint16_t)cnt_blk - 4, 0xFFFF);
		outw((uint16_t)cnt_blk, cmd);
	}

	while(1) { __asm__ volatile ("cli; hlt"); }
	return true;
}

static bool acpi_reboot(kernel_power_dev_t* dev) {
	(void)dev;

	if (fadt && (fadt->flags & (1 << 10))) {
		uintptr_t addr = (uintptr_t)fadt->reset_reg.address;
		uint8_t val = fadt->reset_value;

		if (fadt->reset_reg.address_space_id == 0) { // mmio
			*(volatile uint8_t*)(addr + hhdm_offset) = val;
		} else { // io
			outb((uint16_t)addr, val);
		}
	}

	outb(0xCF9, 0x06);
	outb(0x64, 0xFE);

	while(1) { __asm__ volatile ("cli; hlt"); }
	return true;
}

void acpi_power_init(void) {
	if (UNLIKELY(!fadt || fadt_get_pm1a_cnt() == 0)) { return; }

	kernel_power_dev_t* dev = zalloc(sizeof(kernel_power_dev_t));
	memset(dev->model, 0, 32);
	dev->shutdown = acpi_shutdown;
	dev->reboot = acpi_reboot;
	dev->private_data = NULL;

	power_devices[power_devices_c++] = dev;
}

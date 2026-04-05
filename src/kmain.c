#include <stdint.h>
#include "devicetypes.h"
#include "drivers/UART16550.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "kstate.h"
#include "devices.h"
#include "fb_flanterm.h"
#include "cpu/GDT.h"
#include "apic/lapic.h"
#include "apic/ioapic.h"
#include "apic/madt.h"
#include "pci/pci.h"
#include "pci/drivers/rtl8139.h"
#include "pci/drivers/ide.h"
#include "fs/ramfs.h"
#include "scheduler.h"
#include "cpu/smp/init.h"
#include "fs/cuorefs.h"
#include "stdio.h"
#include "ui/installer.h"
#include "_time.h"
#include "cpu/thermal.h"

volatile struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST_ID,
	.revision = 0
};

volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0
};

volatile struct limine_hhdm_request hhdm_req = {
	.id = LIMINE_HHDM_REQUEST_ID,
	.revision = 0
};

volatile struct limine_framebuffer_request framebuffer_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST_ID,
	.revision = 0
};

volatile struct limine_rsdp_request rsdp_request = {
	.id = LIMINE_RSDP_REQUEST_ID,
	.revision = 0
};

volatile struct limine_mp_request mp_request = {
	.id = LIMINE_MP_REQUEST_ID,
	.revision = 0
};

kernel_config_t global_kernel_config = {0};
uint64_t hhdm_offset;

#define FOUND_CONFIG (1ULL << 0)
#define FOUND_INITRAMFS (1ULL << 1)

pci_driver_entry_t pci_discovery_table[] = {
	// Realtek : RTL8139 (0x8139)
	{
		.vendor_id = PCI_VENDOR_REALTEK,
		.device_id = PCI_DEVICE_RTL8139,
		.class_id = 0, .subclass_id = 0,
		.name = "Realtek RTL8139 Ethernet",
		.init = rtl8139_init
	},

	// Intel : i440FX Host Bridge (0x1237)
	{
		.vendor_id = PCI_VENDOR_INTEL,
		.device_id = PCI_DEVICE_I440FX_HB,
		.class_id = 0, .subclass_id = 0,
		.name = "Intel i440FX Host Bridge",
		.init = NULL // no driver for this
	},

	// ANY Vendor : Class 0x01 (Storage) : Subclass 0x01 (IDE)
	{
		.vendor_id = PCI_VENDOR_ANY,
		.device_id = 0,
		.class_id = 0x01, .subclass_id = 0x01,
		.name = "Generic IDE Controller",
		.init = ide_init
	},

	{ .vendor_id = 0, .device_id = 0, .name = NULL, .init = NULL }
};

void panic(const char* header_msg, const char* msg) {
	if (output_devices_c > 0) {
		for (size_t i = 0; i < output_devices_c; i++) {
			kernel_dev_t* dev = output_devices[i];

			if (DEV_CAP_HAS(dev, CAP_ON_ERROR)) {
				dev_puts(dev, "\n*** KERNEL PANIC: ");
				dev_puts(dev, header_msg);
				dev_puts(dev, " ***\n");
				dev_puts(dev, msg);
				dev_puts(dev, "\n\n");
			}
		}
	} else { // fallback to serial for early boot panic's
		const char* prefix = "\n*** KERNEL PANIC: ";
		const char* suffix = " ***\n";
		const char* newline = "\n";

		for (const char* p = prefix; *p; p++) uart16550_putc(*p);
		for (const char* p = header_msg; *p; p++) uart16550_putc(*p);
		for (const char* p = suffix; *p; p++) uart16550_putc(*p);
		for (const char* p = msg; *p; p++) uart16550_putc(*p);
		for (const char* p = newline; *p; p++) uart16550_putc(*p);
	}

	for(;;) { __asm__ ("hlt"); }
}

kernel_dev_t* output_devices[MAX_DEVICES];
size_t output_devices_c = 0;
ramfs_handle_t initramfs;
kernel_dev_t active_disk_device;
bool supported_disk_exists = false; // when a disk we have a driver for is found by pci discovery this will be set to true

void uart16550_console_task(void) { // example task while scheduler is in development
	while (1) {
		char c =uart16550_getc();
		dev_puts(&uart16550_dev, &c);
	}
}

// void test_multicore_task(void *unused) {
// 	UNUSED(unused);
// 	while (1) {
// 		dev_puts(&uart16550_dev, "Multicore test\n");
// 	}
// }

void idle_task(void) { // stub task while scheduler is in development
	while (1) {}
}

void kernel_main(void) {
	struct limine_mp_response *mp_response = mp_request.response;

 	if (mp_response->cpu_count < 2) {
		logbuf_write("[ SMP  ] Only 1 CPU detected\n");
		logbuf_write(" + [ SMP  ] Starting kernel in single-core mode\n");
	} else {
		logbuf_write("[ SMP  ] Multiple processor's found\n");

		for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
			struct limine_mp_info *cpu = mp_response->cpus[i];

			if (cpu->lapic_id == mp_response->bsp_lapic_id) {
				continue;
			}

			cpu->goto_address = AP_kstartc;
		}

		while (online_cpu_index < mp_response->cpu_count) {
			__asm__ volatile("pause");
		}
	}

	time_t current_boot = get_epoch();
	cuorefs_file_t* existing = cuorefs_find_file("boottime");

	if (existing) {
		if (existing->size >= sizeof(time_t)) {
			time_t old_epoch = *(time_t*)existing->data;
			logbuf_write("[ BOOT ] last logged in: ");
			logbuf_putint(old_epoch);
			logbuf_write("\n");
		}

		free(existing->data);
		free(existing);

		cuorefs_delete_file("boottime");
	}

	// save the current boot time to filesystem
	cuorefs_add_file("boottime", &current_boot, sizeof(time_t));

	logbuf_flush(&uart16550_dev);
	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	scheduler_init();
	scheduler_create_task(uart16550_console_task, 1);
	scheduler_create_task(idle_task, 2);

	// mailbox_send((uint8_t)get_idle_core(), test_multicore_task, NULL);

	scheduler_start();

	for(;;) { __asm__ ("hlt"); }
}

// 256 pages = 1MB
#define HEAP_PAGES 256
#define HEAP_SIZE (HEAP_PAGES << 12)

void _kstartc(void) {
	hhdm_offset = hhdm_req.response->offset;

	acpi_init();
	madt_init();
	lapic_init(madt_get_lapic_base() + hhdm_offset);

	uint8_t my_id = (uint8_t)lapic_get_id();
	cpu_control_block_t *my_cpu = &cpus[my_id];

	my_cpu->dts_support = does_cpu_support_dts();
	my_cpu->lapic_id = my_id;

	if (my_cpu->dts_support) {
		my_cpu->thermal = thermal_read();
	}

	my_cpu->status = CPU_BUSY; // bsp is always busy

	online_cpus[online_cpu_index] = my_cpu;
	__atomic_fetch_add(&online_cpu_index, 1, __ATOMIC_SEQ_CST);

	gdt_init();
	idt_init();

	if (module_request.response->module_count <= 0) {
		logbuf_write("[WARN] initramfs not found. defaulting to hardcoded config\n");
		global_kernel_config.magic = HARDCODED_CONFIG_MAGIC;
		global_kernel_config.epoch = 1774460058;
		global_kernel_config.debug = 1;
		global_kernel_config.flanterm_is_debug_interface = 0;
		global_kernel_config.uart16550_is_debug_interface = 1;
	} else {
		initramfs = ramfs_init(module_request.response->modules[0]->address);
		ramfs_file_t config = ramfs_get_file(&initramfs, "cuore.conf.bin");
		if (config.data != NULL && config.size >= sizeof(kernel_config_t)) {
			kernel_config_t *ptr = (kernel_config_t *)config.data;
			if (ptr->magic == CONFIG_MAGIC) {
				global_kernel_config = *ptr;
			} else {
				panic("CONFIG ERROR", "invalid kernel config magic number");
			}
		} else {
			panic("CONFIG ERROR", "kernel config not found or invalid");
		}
	}

	pma_init();
	uart16550_init();

	ioapic_init(madt_get_ioapic_base() + hhdm_offset);

	uintptr_t phys_addr = pma_alloc_pages(HEAP_PAGES); // 256 pages = 1MB
	void* virt_addr = (void*)(phys_addr + hhdm_req.response->offset);
	heap_init(virt_addr, HEAP_SIZE);

	pci_init();
	logbuf_flush(&uart16550_dev);

	_c_flanterm_init(framebuffer_request.response->framebuffers[0]);

	if (glob_cuorefs_partition == NULL && supported_disk_exists) {
		installer_show_menu();
	}

	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	__asm__ volatile ("sti");

	// start the kernel
	kernel_main();
}

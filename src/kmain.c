#include <stdint.h>
#include "drivers/UART16550.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "kstate.h"
#include "devices.h"
#include "fb_flanterm.h"
#include "ansi_helpers.h"
#include "cpu/GDT.h"
#include "apic/lapic.h"
#include "apic/ioapic.h"
#include "apic/madt.h"
#include "pci/pci.h"
#include "pci/drivers/rtl8139.h"
#include "pci/drivers/ide.h"
#include "drivers/ramfs.h"
#include "scheduler.h"

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
				const char *style = GET_ANSI_STYLE(dev, ANSI_4B_PANIC, ANSI_8B_PANIC, ANSI_24B_PANIC);

				if (style) dev_puts(dev, style);

				dev_puts(dev, "\n*** KERNEL PANIC: ");
				dev_puts(dev, header_msg);
				dev_puts(dev, " ***\n");
				dev_puts(dev, msg);
				dev_puts(dev, "\n\n");

				if (style) dev_puts(dev, ANSI_RESET);
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

void uart16550_console_task(void) { // example task while scheduler is in development
	while (1) {
		char c =uart16550_getc();
		dev_puts(&uart16550_dev, &c);
	}
}

void idle_task(void) { // stub task while scheduler is in development
	while (1) {}
}

void kernel_main(void) {
	struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
	_c_flanterm_init(fb);

	scheduler_init();

	scheduler_create_task(uart16550_console_task, 1);
	scheduler_create_task(idle_task, 2);

	logbuf_flush(&flanterm_dev);
	logbuf_flush(&uart16550_dev);
	logbuf_clear();

	// hand off control to the scheduler
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

	lapic_init(madt_get_lapic_base() + hhdm_offset);
	ioapic_init(madt_get_ioapic_base() + hhdm_offset);

	ioapic_map_irq(4, 36, 0, IOAPIC_TRIGGER_EDGE | IOAPIC_POLARITY_HIGH); // COM1 > vector 36 > cpu 0

	uintptr_t phys_addr = pma_alloc_pages(HEAP_PAGES); // 256 pages = 1MB
	void* virt_addr = (void*)(phys_addr + hhdm_req.response->offset);
	heap_init(virt_addr, HEAP_SIZE);

	pci_init();

	__asm__ volatile ("sti");

	// start the kernel
	kernel_main();
}

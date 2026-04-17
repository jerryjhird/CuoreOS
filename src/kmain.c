#include <stdint.h>
#include "drivers/HPET.h"
#include "drivers/UART16550.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "kstate.h"
#include "devices.h"
#include "fb_flanterm.h"
#include "cpu/GDT.h"
#include "apic/lapic.h"
#include "apic/ioapic.h"
#include "apic/madt.h"
#include "pci/pci.h"
#include "pci/drivers/ide.h"
#include "fs/ramfs.h"
#include "scheduler.h"
#include "cpu/smp/init.h"
#include "fs/cuorefs.h"
#include "stdio.h"
#include "sync.h"
#include "ui/installer.h"
#include "_time.h"
#include "cpu/thermal.h"
#include "cpu/MSR.h"
#include "bitmask.h"

volatile struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST_ID,
	.revision = 0
};

volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0
};

volatile struct limine_executable_address_request executable_request = {
	.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
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

uint64_t hhdm_offset;

pci_driver_entry_t pci_discovery_table[] = {
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
		#ifdef KERNEL_MOD_IDE_ENABLED
			.init = ide_init
		#else
			.init = NULL
		#endif
	},
	{ .vendor_id = 0, .device_id = 0, .name = NULL, .init = NULL }
};

void panic(const char* header_msg, const char* msg) {
	if (char_devices_c > 0) {
		for (size_t i = 0; i < char_devices_c; i++) {
			kernel_char_dev_t* dev = char_devices[i];

			if (BIT_CHECK(dev->DevCAP, CHAR_DEV_CAP_ON_ERROR) && dev->initialized) {
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

kernel_char_dev_t* char_devices[MAX_CHAR_DEVICES];
size_t char_devices_c = 0;

kernel_disk_dev_t* disk_devices[MAX_DISK_DEVICES];
size_t disk_devices_c = 0;

ramfs_handle_t initramfs;
spinlock_t uart_spinlock = SPINLOCK_INIT;
bool supported_disk_exists = false; // when a disk we have a driver for is found by pci discovery this will be set to true

static void uart16550_console_task(void) {
	while (1) {
		char c =uart16550_getc();
		dev_puts(&uart16550_dev, &c);
	}
}

static void idle_task(void) {
	while (1) {}
}

extern void AP_kentry(struct limine_mp_info *mp);

static void kernel_main(void) {
	struct limine_mp_response *mp_response = mp_request.response;

	time_init();

 	if (mp_response->cpu_count < 2) {
		logbuf_write("[ SMP  ] Only 1 CPU detected\n");
		logbuf_write("[ SMP  ] Starting kernel in single-core mode\n\n");
	} else {
		logbuf_write("[ SMP  ] Multiple processor's found\n");

		for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
			struct limine_mp_info *cpu = mp_response->cpus[i];

			void* ap_stack = malloc(AP_STACK_SIZE);
			cpu->extra_argument = (uint64_t)ap_stack + AP_STACK_SIZE;

			if (cpu->lapic_id == mp_response->bsp_lapic_id) {
				continue;
			}

			cpu->goto_address = AP_kentry;
		}

		while (online_cpu_index < mp_response->cpu_count) {
			__asm__ volatile("pause");
		}
	}

	logbuf_write("[ BOOT ] Time it took to boot (ms): "); logbuf_putint(hpet_get_ms()); logbuf_write("\n");
	logbuf_flush(&uart16550_dev);
	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	// commented out until we can fully implement working SMP
	// mailbox_send(get_idle_core(), time_sync, NULL);

	scheduler_init();
	scheduler_create_task(uart16550_console_task, 1);
	scheduler_create_task(idle_task, 2);

	scheduler_start();

	for(;;) { __asm__ ("hlt"); }
}

// 256 pages = 1MB
#define HEAP_PAGES 256
#define HEAP_SIZE (HEAP_PAGES << 12)

void _kstartc(void);

__attribute__((used))
void _kstartc(void) {
	hhdm_offset = hhdm_req.response->offset;

	acpi_init();
	madt_init();
	hpet_init();

	// lapic stuff
	lapic_init(madt_get_lapic_base() + hhdm_offset);
	uint8_t hardware_id = (uint8_t)lapic_get_id();

	gdt_init();
	idt_init();
	pma_init();

	uintptr_t phys_addr = pma_alloc_pages(HEAP_PAGES); // 256 pages = 1MB
	void* virt_addr = (void*)(phys_addr + hhdm_req.response->offset);
	heap_init(virt_addr, HEAP_SIZE);

	logical_coreid_t idx = __atomic_fetch_add(&online_cpu_index, 1, __ATOMIC_SEQ_CST);
	cpu_control_block_t *my_cpu = zalloc(sizeof(cpu_control_block_t));
	logical_indexed_cpu_list[idx] = my_cpu;
	my_cpu->self = my_cpu;

	__asm__ volatile ("wrmsr" : : "c"(MSR_GS_BASE), "a"((uint32_t)(uint64_t)my_cpu), "d"((uint32_t)((uint64_t)my_cpu >> 32)));

	my_cpu->logical_id = idx;
	my_cpu->lapic_id = hardware_id;
	my_cpu->ticks = 1;
	my_cpu->dts_support = does_cpu_support_dts();

	if (my_cpu->dts_support) {my_cpu->thermal = thermal_read();}
	my_cpu->status = CPU_BUSY; // BSP is always busy

	uart16550_init();
	dev_puts(&uart16550_dev, "\033[2J\033[H"); // ensure we dont overwrite things like the qemu dvd rom blob

	ioapic_init(madt_get_ioapic_base() + hhdm_offset);

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

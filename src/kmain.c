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
#include "acpi/power.h"
#include "fs/ramfs.h"
#include "scheduler.h"
#include "cpu/smp/init.h"
#include "stdio.h"
#include "_time.h"
#include "cpu/MSR.h"
#include "bitmask.h"
#include "cpu/rdrand.h"
#include "multimedia/beep.h"
#include "acpi/mcfg.h"
#include "acpi/fadt.h"
#include "builtinabs.h"
#include "disk/diskinit.h"
#include "tests.h"
#include "gui/wm.h"
#include "gui/fb.h"

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

#ifndef KERNEL_BUILD_SIGNATURE
#define KERNEL_BUILD_SIGNATURE 0x0ULL
#endif

__attribute__((section(".csig"), used))
const uint64_t compile_signature = KERNEL_BUILD_SIGNATURE;

uint64_t hhdm_offset;

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

ramfs_handle_t initramfs;
linear_framebuffer_t gfb_limine_framebuffer;
bool supported_disk_exists = false; // when a disk we have a driver for is found by pci discovery this will be set to true

static void uart16550_console_task(void) {
	while (1) {
		char c = uart16550_getc();
		dev_puts(&uart16550_dev, &c);

		#ifdef DEBUG
			if (c == 's') {
				if (LIKELY(power_devices_c > 0 && power_devices[0] != NULL)) {
					power_devices[0]->shutdown(power_devices[0]);
				} else {
					panic("POWER", "no power device registered for shutdown!\n");
				}
			}
			else if (c == 'r') {
				if (LIKELY(power_devices_c > 0 && power_devices[0] != NULL)) {
					power_devices[0]->reboot(power_devices[0]);
				} else {
					panic("POWER", "no power device registered for reboot!\n");
				}
			}
		#endif
	}
}

static void idle_task(void) {
	while (1) {}
}

static void startup_sound_task(void) {
	audio_beep(active_audio_device, 659, 150);
	audio_beep(active_audio_device, 1, 90);
	audio_beep(active_audio_device, 659, 150);
	audio_beep(active_audio_device, 1, 90);
	audio_beep(active_audio_device, 659, 300);
	audio_beep(active_audio_device, 1, 100);
	audio_beep(active_audio_device, 659, 150);
	audio_beep(active_audio_device, 1, 90);
	audio_beep(active_audio_device, 659, 150);
	audio_beep(active_audio_device, 1, 90);
	audio_beep(active_audio_device, 659, 300);
	audio_beep(active_audio_device, 1, 100);
	audio_beep(active_audio_device, 659, 150);
	audio_beep(active_audio_device, 783, 150);
	audio_beep(active_audio_device, 523, 150);
	audio_beep(active_audio_device, 587, 150);
	audio_beep(active_audio_device, 659, 600);
	scheduler_exit_task();
}

extern void AP_kentry(struct limine_mp_info *mp);

static void kernel_main(void) {
	struct limine_mp_response *mp_response = mp_request.response;

	time_init();

	uint64_t cores_to_boot = mp_response->cpu_count;
	if (cores_to_boot > SMP_MAX_CORES) {
		cores_to_boot = SMP_MAX_CORES;
	}

	if (mp_response->cpu_count < 2) {
		logbuf_write("[ SMP  ] Only 1 CPU detected\n");
	} else {
		logbuf_write("[ SMP  ] Multiple processors found\n");

		if (mp_response->cpu_count > SMP_MAX_CORES) {
			logbuf_write("[ WARN ] the config used to compile this kernel limits usage to ");
			logbuf_putint(SMP_MAX_CORES);
			logbuf_write(" of ");
			logbuf_putint(mp_response->cpu_count);
			logbuf_write(" available cores.\n[ WARN ] ");
			logbuf_putint(mp_response->cpu_count - SMP_MAX_CORES);
			logbuf_write(" cores will remain inactive.\n");
		}

		for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
			struct limine_mp_info *cpu = mp_response->cpus[i];

			if (cpu->lapic_id == mp_response->bsp_lapic_id) {
				continue;
			}

			if (i >= cores_to_boot) {
				continue;
			}

			void* ap_stack = malloc(AP_STACK_SIZE);
			cpu->extra_argument = (uint64_t)ap_stack + AP_STACK_SIZE;
			cpu->goto_address = AP_kentry;
		}

		while (__atomic_load_n(&cpu_devices_c, __ATOMIC_ACQUIRE) < cores_to_boot) {
			__asm__ volatile("pause");
		}
	}

	// random test
	uint64_t random = 0;
	if (!rdrand_supported()) {
		logbuf_write("[ RNG  ] rdrand not supported\n");
	} else {
		if (rdrand64(&random)) {
			logbuf_write("[ RNG  ] ");
			logbuf_putint(random);
			logbuf_write("\n");
		}
	}

	if (mp_response->cpu_count > 2) {
		mailbox_send_fc(time_sync, NULL);
	}

	logbuf_write("[ KRNL ] Signature: ");
	logbuf_putint(compile_signature);
	logbuf_write("\n");

	logbuf_write("[ BOOT ] Time it took to boot (nano's): "); logbuf_putint(hpet_get_nanos()); logbuf_write("\n");

	logbuf_flush(&uart16550_dev);
	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	if (active_net_device) {
		msleep(10); // give the card time to initalize
		uint8_t frame[60];
		memset(frame, 0, sizeof(frame));

		memset(frame, 0xFF, 6);
		memcpy(frame + 6, active_net_device->mac, 6);

		frame[12] = 0x08;
		frame[13] = 0x00;

		const char* msg = "hello world";
		memcpy(frame + 14, msg, 12);

		active_net_device->send_packet(active_net_device, frame, sizeof(frame));
	}

	scheduler_init();
	scheduler_create_task(uart16550_console_task, 1);
	scheduler_create_task(idle_task, 2);
	scheduler_create_task(startup_sound_task, 3);
	scheduler_start();

	for(;;) { __asm__ ("hlt"); }
}

// 256 pages = 1MB
#define HEAP_PAGES 4096
#define HEAP_SIZE (HEAP_PAGES << 12)

void _kstartc(void);

__attribute__((used))
void _kstartc(void) {
	hhdm_offset = hhdm_req.response->offset;

	acpi_init();
	fadt_init();
	madt_init();
	hpet_init();

	// lapic stuff
	lapic_init(madt_get_lapic_base() + hhdm_offset);
	uint8_t hardware_id = (uint8_t)lapic_get_id();

	gdt_init();
	idt_init();
	pma_init();

	if (module_request.response->module_count <= 0) {
		logbuf_write("[WARN] initramfs not found.\n");
	} else {
		initramfs = ramfs_init(module_request.response->modules[0]->address);
	}

	heap_init((void*)KERNEL_HEAP_START, HEAP_SIZE);

	uint32_t idx = __atomic_fetch_add(&cpu_devices_c, 1, __ATOMIC_SEQ_CST);
	kernel_cpu_dev_t *my_cpu = zalloc(sizeof(kernel_cpu_dev_t));
	cpu_devices[idx] = my_cpu;
	my_cpu->self = my_cpu;

	__asm__ volatile ("wrmsr" : : "c"(MSR_GS_BASE), "a"((uint32_t)(uint64_t)my_cpu), "d"((uint32_t)((uint64_t)my_cpu >> 32)));

	my_cpu->logical_id = idx;
	my_cpu->lapic_id = hardware_id;
	my_cpu->ticks = 1;

	my_cpu->status = CPU_BUSY; // BSP is always busy

	uart16550_init();
	dev_puts(&uart16550_dev, "\033[2J\033[H"); // ensure we dont overwrite things like the qemu dvd rom blob

	ioapic_init(madt_get_ioapic_base() + hhdm_offset);
	mcfg_init();

	acpi_power_init();

	pci_init();

	// scan disks previously found by pci discovery
	for (size_t i = 0; i < disk_devices_c; i++) {
		kernel_disk_dev_t* dev = disk_devices[i];

		if (dev == NULL) continue;

		#ifdef DO_KDEVTESTS
			if (!dev__test__disk(dev)) {
				panic("DISK", "Test Failed");
			}
		#endif

		logbuf_write("[ DISK ] Scanning ");
		logbuf_write(dev->model);
		logbuf_write("\n");

		generic_disk_init(dev);
	}

	logbuf_flush(&uart16550_dev);

	GENERIC_FB_FROM_LIMINE_FB(&gfb_limine_framebuffer, framebuffer_request.response->framebuffers[0]);

	wm_init(&gfb_limine_framebuffer);

	int terminal_win_w = gfb_limine_framebuffer.width - (20 * 2);
	int terminal_win_h = gfb_limine_framebuffer.height - (20 * 2) - WM_TITLEBAR_HEIGHT;
	int terminal_win_x = 20; int terminal_win_y = 20;

	window_t* terminal_window = wm_create_window(terminal_win_x, terminal_win_y, terminal_win_w, terminal_win_h);

	_c_flanterm_init(terminal_window->buffer);

	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	__asm__ volatile ("sti");

	// start the kernel
	kernel_main();
}

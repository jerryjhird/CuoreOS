#include <stdint.h>
#include "panic.h"
#include "drivers/HPET.h"
#include "drivers/UART16550.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "devices.h"
#include "graphics/ui/fb_flanterm.h"
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
#include "abs.h"
#include "disk/diskinit.h"
#include "tests.h"
#include "graphics/ui/wm.h"
#include "graphics/fb.h"
#include "net/link/eth.h"
#include "net/protocol/ntp.h"
#include "net/protocol/dns.h"
#include "net/protocol/telnet.h"
#include "net/arp.h"

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
				dev_printf(dev, "\n*** KERNEL PANIC: %s ***\n%s\n\n", header_msg, msg);
			}
		}
	} else {
		const char* prefix = "\n*** KERNEL PANIC: ";
		const char* suffix = " ***\n";
		const char* newline = "\n\n";

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

	if (mp_response->cpu_count >= 2) {
		logbuf_write("[ SMP  ] Multiple processors found\n");

		if (mp_response->cpu_count > SMP_MAX_CORES) {
			logbuf_printf("[ WARN ] the config used to compile this kernel limits usage to %zu of %zu available cores.\n" "[ WARN ] %zu cores will remain inactive.\n", (size_t)SMP_MAX_CORES, (size_t)mp_response->cpu_count, (size_t)(mp_response->cpu_count - SMP_MAX_CORES));
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
			logbuf_printf("[ RNG  ] %llu\n", random);
		}
	}

	if (mp_response->cpu_count > 2) {
		mailbox_send_fc(time_sync, NULL);
	}

	logbuf_printf("[ KRNL ] Signature: %llu\n", (unsigned long long)compile_signature);
	logbuf_printf("[ BOOT ] Time it took to boot (nano's): %llu\n", (unsigned long long)hpet_get_nanos());

	if (active_net_device) {
		eth_init(active_net_device);

		active_net_device->ip_addr	  = IP_ADDR(10, 0, 2, 15);
		active_net_device->gateway	  = IP_ADDR(10, 0, 2, 2);
		active_net_device->subnet_mask  = IP_ADDR(255, 255, 255, 0);

		arp_send_request(active_net_device, active_net_device->gateway);

		uint32_t google_time = dns_query_blocking(active_net_device, IP_ADDR(8,8,8,8), "time.google.com");
		if (google_time != 0) {
			ntp_send_request(active_net_device, google_time);
		} else {
			dev_puts(&uart16550_dev, "[ DNS ] could not resolve\n");
		}

		// uint32_t STARWARS_SERVER = dns_query_blocking(active_net_device, IP_ADDR(8,8,8,8), "towel.blinkenlights.nl");
		// if (STARWARS_SERVER != 0) {
		// 	telnet_client(active_net_device, STARWARS_SERVER, 23);
		// } else {
		// 	dev_puts(&uart16550_dev, "[ DNS ] could not resolve\n");
		// }
	}

	logbuf_flush(&uart16550_dev);
	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	scheduler_init();
	scheduler_create_task(uart16550_console_task, 1);
	scheduler_create_task(idle_task, 2);
	scheduler_create_task(startup_sound_task, 3);
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
		ramfs_init(&initramfs, module_request.response->modules[0]->address);
	}

	heap_init((void*)0xFFFFFFFF90000000, HEAP_SIZE);

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

		logbuf_printf("[ DISK ] Scanning %s\n", dev->model);
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

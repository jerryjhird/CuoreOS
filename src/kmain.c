#include <stdint.h>
#include "Config.h"
#include "device/devreg.h"
#include "panic.h"
#include "drivers/HPET.h"
#include "drivers/UART16550.h"
#include "drivers/E9.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "graphics/ui/fb_flanterm.h"
#include "cpu/GDT.h"
#include "apic/lapic.h"
#include "apic/ioapic.h"
#include "apic/madt.h"
#include "pci/pci.h"
#include "fs/ramfs.h"
#include "scheduler.h"
#include "cpu/smp/init.h"
#include "stdio.h"
#include "_time.h"
#include "cpu/MSR.h"
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
#include "mem/vmm.h"
#include "net/arp.h"
#include "binfmt/elf64.h"
#include "symtable.h"
#include "cpu/coreinfo.h"
#include "mem/paging.h"
#include "mem/cmm.h"
#include "cmdline.h"

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

volatile struct limine_executable_cmdline_request cmdline_request = {
	.id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
	.revision = 0
};

#ifndef KERNEL_BUILD_SIGNATURE
#define KERNEL_BUILD_SIGNATURE 0x0ULL
#endif

__attribute__((section(".csig"), used))
const uint64_t compile_signature = KERNEL_BUILD_SIGNATURE;

uint64_t hhdm_offset;

void panic(const char* header_msg, const char* msg) {
	__asm__ volatile ("cli");

	bool printed = false;

	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i].type != CHAR_DEV) continue;

		kernel_char_dev_t* dev = (kernel_char_dev_t*)registry[i].dev_ptr;

		if (BIT_CHECK(dev->DevCAP, CHAR_IS_ERROR_HANDLER) && dev->initialized) {
			dev_printf(dev, "\n*** KERNEL PANIC: %s ***\n%s\n\n", header_msg, msg);
			printed = true;
		}
	}

	if (!printed) {
		const char* prefix = "\n*** KERNEL PANIC: ";
		const char* suffix = " ***\n";
		const char* newline = "\n\n";

		for (const char* p = prefix; *p; p++) uart16550_putc(*p);
		for (const char* p = header_msg; *p; p++) uart16550_putc(*p);
		for (const char* p = suffix; *p; p++) uart16550_putc(*p);
		for (const char* p = msg; *p; p++) uart16550_putc(*p);
		for (const char* p = newline; *p; p++) uart16550_putc(*p);
	}

	for(;;) { __asm__ volatile ("hlt"); }
}

ramfs_handle_t initramfs;
linear_framebuffer_t gfb_limine_framebuffer;

static void uart16550_console_task(void) {
	while (1) {
		char c = uart16550_getc();
		dev_puts(debug_dev, &c);
	}
}

static void startup_sound_task(void) {
	kernel_audio_dev_t* audio_dev = (kernel_audio_dev_t*)device_find_first(AUDIO_DEV);

	if (UNLIKELY(!audio_dev)) {
		scheduler_exit_task();
		return;
	}

	audio_beep(audio_dev, 659, 150);
	// audio_beep(audio_dev, 1, 90);
	// audio_beep(audio_dev, 659, 150);
	// audio_beep(audio_dev, 1, 90);
	// audio_beep(audio_dev, 659, 300);
	// audio_beep(audio_dev, 1, 100);
	// audio_beep(audio_dev, 659, 150);
	// audio_beep(audio_dev, 1, 90);
	// audio_beep(audio_dev, 659, 150);
	// audio_beep(audio_dev, 1, 90);
	// audio_beep(audio_dev, 659, 300);
	// audio_beep(audio_dev, 1, 100);
	// audio_beep(audio_dev, 659, 150);
	// audio_beep(audio_dev, 783, 150);
	// audio_beep(audio_dev, 523, 150);
	// audio_beep(audio_dev, 587, 150);
	// audio_beep(audio_dev, 659, 600);

	scheduler_exit_task();
}

extern void AP_kentry(struct limine_mp_info *mp);

coreinfo_t *cpu_blocks[SMP_MAX_CORES];
uint32_t cpu_blocks_c = 0;
uint64_t kernel_pml4_phys = 0;

static void kernel_main(void) {
	struct limine_mp_response *mp_response = mp_request.response;

	time_init();

	uint64_t maxcpu_req = cmdline_get_uint("maxcpus", SMP_MAX_CORES);
	if (maxcpu_req == 0) maxcpu_req = 1;

	if (maxcpu_req > SMP_MAX_CORES) {
		logbuf_warn("[ SMP  ] CMDLINE's maxcpus is set higher than SMP_MAX_CORES. maxcpu will be set to SMP_MAX_CORES, increase SMP_MAX_CORES in build config\n");
		maxcpu_req = SMP_MAX_CORES;
	}

	uint64_t cores_to_boot = mp_response->cpu_count;
	if (cores_to_boot > SMP_MAX_CORES) {
		cores_to_boot = SMP_MAX_CORES;
	}

	if (cores_to_boot > maxcpu_req) {
		cores_to_boot = maxcpu_req;
	}

	__atomic_fetch_add(&cpu_online_count, 1, __ATOMIC_SEQ_CST);

	if (mp_response->cpu_count >= 2) {
		logbuf_info("[ SMP  ] Multiple processors found\n");

		if (mp_response->cpu_count > cores_to_boot) {
			const char* reason = (maxcpu_req < SMP_MAX_CORES) ? "the arguments used to boot this kernel" : "the config used to compile this kernel";

			logbuf_warn("[ SMP  ] %s limits usage to %zu of %zu available cores.\n", reason, (size_t)cores_to_boot, (size_t)mp_response->cpu_count);
			logbuf_warn("[ SMP  ] %zu cores will remain inactive.\n", (size_t)(mp_response->cpu_count - cores_to_boot));
		}

		for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
			struct limine_mp_info *cpu = mp_response->cpus[i];

			if (cpu->lapic_id == mp_response->bsp_lapic_id) { continue; }
			if (i >= cores_to_boot) { continue; }

			void* ap_stack = malloc(AP_STACK_SIZE);
			cpu->extra_argument = (uint64_t)ap_stack + AP_STACK_SIZE;
			cpu->goto_address = AP_kentry;
		}

		while (__atomic_load_n(&cpu_online_count, __ATOMIC_ACQUIRE) < cores_to_boot) {
			__asm__ volatile("pause");
		}

		__asm__ volatile("" ::: "memory");

		for (uint32_t i = 0; i < cores_to_boot; i++) {
			if (cpu_blocks[i] == NULL) {
				panic("SMP", "cpu core registered as online but block is NULL");
			}
			irq_install_handler(i, 40, AP_ipi_wakeup_irq);
			irq_install_handler(i, 32, AP_clock_tick_irq);
		}
	}

	if (mp_response->cpu_count > 2) {
		mailbox_send_fc(time_sync, NULL);
	}

	logbuf_info("[ KRNL ] Signature: %llu\n", (unsigned long long)compile_signature);
	logbuf_info("[ BOOT ] Time it took to boot (nano's): %llu\n", (unsigned long long)hpet_get_nanos());

	kernel_net_dev_t* net_dev = (kernel_net_dev_t*)device_find_first(NET_DEV);

	if (net_dev) {
		eth_init(net_dev);

		net_dev->ip_addr	  = IP_ADDR(10, 0, 2, 15);
		net_dev->gateway	  = IP_ADDR(10, 0, 2, 2);
		net_dev->subnet_mask  = IP_ADDR(255, 255, 255, 0);

		arp_send_request(net_dev, net_dev->gateway);

		uint32_t google_time = dns_query_blocking(net_dev, IP_ADDR(8,8,8,8), "time.google.com");
		if (google_time != 0) {
			ntp_send_request(net_dev, google_time);
		} else {
			dev_puts(debug_dev, "[ DNS ] could not resolve\n");
		}
	}

	logbuf_flush(debug_dev);
	logbuf_flush(&flanterm_dev);
	logbuf_clear();

	scheduler_init();

	task_t *uart_console_t = scheduler_create_task(uart16550_console_task);
	scheduler_enroll_task(uart_console_t);

	ramfs_file_t init = ramfs_get_file(&initramfs, "init.elf");
	if (init.data != NULL) {
		task_t *init_system_t = elf64_alloc(init.data, init.size);
		scheduler_enroll_task(init_system_t);
	}

	task_t *startup_sound_task_t = scheduler_create_task(startup_sound_task);
	scheduler_enroll_task(startup_sound_task_t);

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
	kernel_pml4_phys = vmm_get_pml4();

	cmdline_init();

	const char* debugout = cmdline_get_string("debugout");

	if (debugout != NULL && strcmp(debugout, "e9") == 0) {
		E9_init();
		debug_dev = &e9_dev;
	} else {
		// default
		uart16550_init();
		debug_dev = &uart16550_dev;
	}

	// initramfs
	if (module_request.response->module_count <= 0) {
		logbuf_warn("initramfs not found.\n");
	} else {
		ramfs_init(&initramfs, module_request.response->modules[0]->address);
	}

	ramfs_file_t sym_file = ramfs_get_file(&initramfs, "symtable.data");

	if (sym_file.data == NULL || sym_file.size == 0) {
		panic("SYMTABLE", "failed to find 'symtable.data' in initramfs");
	} else {
		symtable_init(sym_file.data, (size_t)sym_file.size);
	}

	dev_puts(debug_dev, "\033[2J\033[H");

	acpi_init();
	fadt_init();
	madt_init();
	hpet_init();

	gdt_init();
	idt_init();

	cmm_init();
	pma_init();
	vmm_init();

	// lapic stuff
	uintptr_t lapic_phys = madt_get_lapic_base();
	uint64_t* pml4_virt = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE;

	vmm_map_page(pml4_virt, LAPIC_VIRTUAL_BASE, lapic_phys, flags);
	lapic_init(LAPIC_VIRTUAL_BASE);
	uint8_t hardware_id = (uint8_t)lapic_get_id();

	heap_init(HEAP_SIZE);

	// cpu block
	coreinfo_t *my_cpu = zalloc(sizeof(coreinfo_t));
	cpu_blocks[cpu_blocks_c] = my_cpu;
	my_cpu->self = my_cpu;

	WRITE_MSR(MSR_GS_BASE, (uint64_t)my_cpu);

	my_cpu->logical_id = 0;
	my_cpu->lapic_id = hardware_id;
	my_cpu->ticks = 1;
	my_cpu->status = CPU_BUSY; // BSP is always busy

	cpu_blocks_c++;

	uart16550_init_late();

	ioapic_init(madt_get_ioapic_base() + hhdm_offset);
	mcfg_init();

	logbuf_flush(debug_dev);

	// wm stuff
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

	// pci stuff
	pci_init();

	// scan disks previously found by pci discovery
	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i].type != DISK_DEV) continue;

		kernel_disk_dev_t* dev = (kernel_disk_dev_t*)registry[i].dev_ptr;
		if (!dev) continue;

		#ifdef DO_KDEVTESTS
			if (!dev__test__disk(dev)) {
				panic("DISK", "Test Failed");
			}
		#endif

		generic_disk_init(dev);
	}

	// start the kernel
	kernel_main();
}

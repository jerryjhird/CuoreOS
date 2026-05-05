#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cpu/IRQ.h"
#include "cpu/smp/mailbox.h"
#include "scheduler.h"

// character device capabilities
#define CHAR_DEV_CAP_ON_ERROR (1ULL << 0) // device should be used to display error's (e.g cpu exceptions, panics etc)

// generic text output device (like a terminal or serial port)
typedef struct kernel_char_dev_t {
	uint8_t DevCAP;
	void (*putc)(char c);
	bool initialized;
} kernel_char_dev_t;

// cpu core
typedef enum { CPU_IDLE = 0, CPU_BUSY, CPU_HALTED, CPU_PANIC } cpu_status_t;
typedef struct kernel_cpu_dev_t {
	struct kernel_cpu_dev_t *self; // @ gs:0
	task_t *current_task; // @ gs:8
	uint32_t logical_id;
	uint32_t lapic_id;
	uint64_t ticks;
	volatile cpu_status_t status;
	mailbox_t mailbox;

	irq_vector_chain_t routines[256];
	uint64_t irq_stats[256];
} __attribute__((aligned(64))) kernel_cpu_dev_t;

typedef struct kernel_disk_dev_t {
	struct partition_s* partitions;
	char model[41]; // for model name
	uint64_t total_sectors;
   	uint16_t port_base;
	uint8_t (*read_sector)(struct kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer);
	uint8_t (*write_sector)(struct kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer);
	void* private_data;
} kernel_disk_dev_t;

typedef struct kernel_audio_dev_t {
	char model[32];

	// hardware state
	uint32_t sample_rate;
	uint8_t channels; // 1 for mono, 2 for stereo
	uint8_t bit_depth;

	void (*play)(struct kernel_audio_dev_t* dev, void* buffer, size_t size);
	void (*stop)(struct kernel_audio_dev_t* dev);
	void (*set_volume)(struct kernel_audio_dev_t* dev, uint8_t volume); // 0-100

	// hook(hook_args) is called when audio playback is finished
	void (*volatile hook)(void*);
	void* volatile hook_args;

	volatile bool is_playing;
	void* private_data;
} kernel_audio_dev_t;

typedef struct kernel_power_dev_t {
	char model[32];
	bool (*shutdown)(struct kernel_power_dev_t* dev);
	bool (*reboot)(struct kernel_power_dev_t* dev);
	void* private_data;
} kernel_power_dev_t;

typedef struct kernel_net_dev_t {
	char model[32];
	uint8_t mac[6];

	uint32_t ip_addr;
	uint32_t subnet_mask;
	uint32_t gateway;

	// driver interface
	void (*send_packet)(struct kernel_net_dev_t* dev, void* data, size_t len);
	void (*set_promiscuous)(struct kernel_net_dev_t* dev, bool enabled);

	// hook for networking stack
	void (*on_received)(struct kernel_net_dev_t* dev, void* data, size_t len);

	void* private_data;
	bool link_up;
} kernel_net_dev_t;

typedef enum {
	VOLATILE, // standard RAM like
	PERSISTENT, // NVDIMM or flash
	MMIO_GATE, // bridge to other hardware
	HOST_TO_GUEST // e.g ivshmem
} extmem_type_t;

typedef struct kernel_extmem_dev_t {
	char model[32];
	extmem_type_t type;
	void*	 virt_addr;
	uintptr_t phys_addr;
	size_t	size;
	void* private_data;
} kernel_extmem_dev_t;

void dev_puts(kernel_char_dev_t* dev, const char* s);
void dev_putint(kernel_char_dev_t* dev, uint64_t n);

extern kernel_char_dev_t uart16550_dev;
extern kernel_char_dev_t flanterm_dev;

extern kernel_char_dev_t* char_devices[MAX_CHAR_DEVICES];
extern size_t char_devices_c;

extern kernel_disk_dev_t* disk_devices[MAX_DISK_DEVICES];
extern size_t disk_devices_c;

extern kernel_power_dev_t* power_devices[MAX_POWER_DEVICES];
extern size_t power_devices_c;

extern kernel_extmem_dev_t* extmem_devices[MAX_EXTMEM_DEVICES];
extern size_t extmem_devices_c;

#define GET_CURRENT_CPU(target) __asm__ volatile ("mov %%gs:0, %0" : "=r"(target))
#define GET_CURRENT_TASK(target) __asm__ volatile ("mov %%gs:8, %0" : "=r"(target))
#define SET_CURRENT_TASK(bootstrap_task) __asm__ volatile ("mov %0, %%gs:8" : : "r"(bootstrap_task));
extern kernel_cpu_dev_t* cpu_devices[SMP_MAX_CORES];
extern volatile uint32_t cpu_devices_c;

extern kernel_net_dev_t* active_net_device;
extern kernel_audio_dev_t* active_audio_device;

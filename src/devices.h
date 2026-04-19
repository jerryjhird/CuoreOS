#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// character device capabilities
#define CHAR_DEV_CAP_ON_ERROR (1ULL << 0) // device should be used to display error's (e.g cpu exceptions, panics etc)

// generic text output device (like a terminal or serial port)
typedef struct kernel_char_dev_t {
	uint8_t DevCAP;
	void (*putc)(char c);
	bool initialized;
} kernel_char_dev_t;

typedef struct kernel_disk_dev_t {
	struct partition_s* partitions;
	char model[41]; // for model name
	uint64_t total_sectors;
   	uint16_t port_base;
	uint8_t (*read_sector)(struct kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer);
	uint8_t (*write_sector)(struct kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer);
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

void dev_puts(kernel_char_dev_t* dev, const char* s);
void dev_putint(kernel_char_dev_t* dev, uint64_t n);

extern kernel_char_dev_t uart16550_dev;
extern kernel_char_dev_t flanterm_dev;

extern kernel_char_dev_t* char_devices[MAX_CHAR_DEVICES];
extern size_t char_devices_c;

extern kernel_disk_dev_t* disk_devices[MAX_DISK_DEVICES];
extern size_t disk_devices_c;

extern kernel_audio_dev_t* active_audio_device;

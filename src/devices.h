#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// generic device type
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

// what to feed the device
#define CHAR_DEV_CAP_ON_ERROR (1ULL << 0) // device should be used to display unrecoverable error's (e.g cpu exceptions)

void dev_puts(kernel_char_dev_t* dev, const char* s);
void dev_putint(kernel_char_dev_t* dev, uint64_t n);

extern kernel_char_dev_t uart16550_dev;
extern kernel_char_dev_t flanterm_dev;

extern kernel_char_dev_t* char_devices[MAX_CHAR_DEVICES];
extern size_t char_devices_c;

extern kernel_disk_dev_t* disk_devices[MAX_DISK_DEVICES];
extern size_t disk_devices_c;

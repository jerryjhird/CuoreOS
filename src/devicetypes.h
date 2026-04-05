#pragma once

#include <stdint.h>
#include <stddef.h>

struct kernel_dev_t;
typedef struct kernel_dev_t kernel_dev_t;

// generic device type
typedef struct kernel_dev_t {
	uint64_t DevCAP;
	void* ctx; // device specific context

	// text output
	void (*putc)(char c);

	// disk stuff
	struct partition_s* partitions;
	uint64_t total_sectors;
	uint8_t (*read_sector)(struct kernel_dev_t* dev, uint32_t lba, uint16_t* buffer);
	uint8_t (*write_sector)(struct kernel_dev_t* dev, uint32_t lba, uint16_t* buffer);

} kernel_dev_t;

#define SETUP_OUTPUT_DEVICE(name, _cap, _putc, _read_sectors, _write_sectors) \
	kernel_dev_t name = {										\
		.DevCAP = (_cap),										\
		.putc   = _putc,										 \
		.read_sector = _read_sectors,							\
		.write_sector = _write_sectors,						  \
	}

#define FILL_OUTPUT_DEVICE(ptr, _cap, _putc, _read_sectors, _write_sectors) \
	do {													 \
		(ptr)->DevCAP = (_cap);							  \
		(ptr)->putc   = _putc;							   \
		(ptr)->read_sector = _read_sectors,				  \
		(ptr)->write_sector  = _write;					   \
	} while(0)

#define REGISTER_OUTPUT_DEVICE(ptr, list, counter) \
	do { \
		if ((ptr) != NULL && (counter) < MAX_DEVICES) { \
			(list)[(counter)++] = (ptr); \
		} \
	} while(0)

// what to feed the device
#define CAP_ON_ERROR (1ULL << 0) // device should be used to display unrecoverable error's (e.g cpu exceptions)
#define CAP_ON_DEBUG (1ULL << 1) // device should be used to display debug messages
#define CAP_IS_DISK (1ULL << 2) // device is a disk

#define DEV_CAP_SET(dev_ptr, cap) ((dev_ptr)->DevCAP |= (cap))
#define DEV_CAP_CLEAR(dev_ptr, cap) ((dev_ptr)->DevCAP &= ~(cap))
#define DEV_CAP_HAS(dev_ptr, cap) (((dev_ptr)->DevCAP & (cap)) == (cap))

void dev_puts(kernel_dev_t* dev, const char* s);
void dev_putint(kernel_dev_t* dev, uint64_t n);

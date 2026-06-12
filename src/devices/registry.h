#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
	CHAR_DEV,
	DISK_DEV,
	USB_DEV,
	NET_DEV,
	AUDIO_DEV,
	POWER_DEV,
	EXTMEM_DEV,
	USB_BUS_DEV
} device_type_t;

typedef struct {
	uint32_t id;
	device_type_t type;
	void* dev_ptr;
} device_registry_entry_t;

typedef uint32_t dev_handle_t;

extern device_registry_entry_t registry[512];
extern size_t registry_count;
extern uint32_t next_device_id;

void* device_find(uint32_t id);
void* device_find_first(device_type_t type);
uint32_t device_register(device_type_t type, void* dev_ptr);

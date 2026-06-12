#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mem/dmalloc.h"

#define CHAR_IS_ERROR_HANDLER (1ULL << 0)

typedef struct kernel_char_dev_t {
	uint64_t DevCAP;
	void (*putc)(char c);
	bool initialized;
} kernel_char_dev_t;

typedef struct kernel_disk_dev_t {
	struct partition_s* partitions;
	char model[41]; // for model name
	uint64_t total_sectors;
   	uint16_t port_base;
	uint8_t (*read_sectors)(struct kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer);
	uint8_t (*write_sectors)(struct kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer);
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
	bool initialized;
} kernel_net_dev_t;

typedef enum {
	IVSHMEM_NON_PERSISTENT,
	IVSHMEM_PERSISTENT,
	CXL_TYPE2_ACCEL_MEM,
	CXL_TYPE3_VOLATILE,
	CXL_TYPE3_PERSISTENT
} extmem_type_t;

typedef struct kernel_extmem_dev_t {
	extmem_type_t type;
	uint64_t virt_addr;
	uint64_t phys_addr;
	size_t	size;

	void* private_data;
} kernel_extmem_dev_t;

typedef struct kernel_usb_bus_dev_t kernel_usb_bus_dev_t;
typedef struct kernel_usb_dev_t kernel_usb_dev_t;

// USB device
struct kernel_usb_dev_t {
	uint8_t slot_id;
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t class_code;

	struct kernel_usb_bus_dev_t* bus;

	void (*send_transfer)(struct kernel_usb_dev_t* dev, uint8_t endpoint, void* buffer, size_t len);
	void (*on_event)(struct kernel_usb_dev_t* dev, void* event_trb);

	void* private_data;
};

// USB bus
struct kernel_usb_bus_dev_t {
	uint8_t bus_number;

	struct kernel_usb_dev_t* children[64];

	void (*reset_port)(struct kernel_usb_bus_dev_t* bus, uint8_t port_id);
	void (*poll_bus)(struct kernel_usb_bus_dev_t* bus);

	void* private_data;
};

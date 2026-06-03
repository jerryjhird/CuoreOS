#include "devreg.h"

#include "device/types.h"
#include "panic.h"

device_registry_entry_t registry[512];
size_t registry_count = 0;
uint32_t next_device_id = 1;
kernel_char_dev_t* debug_dev = {0};

void* device_find(uint32_t id) {
	int low = 0, high = (int)registry_count - 1;
	while (low <= high) {
		int mid = low + (high - low) / 2;
		if (registry[mid].id == id) return registry[mid].dev_ptr;
		if (registry[mid].id < id) low = mid + 1;
		else high = mid - 1;
	}
	return NULL;
}

void* device_find_first(device_type_t type) {
	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i].type == type) return registry[i].dev_ptr;
	}
	return NULL;
}

uint32_t device_register(device_type_t type, void* dev_ptr) {
	size_t i = registry_count;
	uint32_t id = next_device_id++;

	if (registry_count >= 512) {
		panic("DEVICE REGISTRY", "OUT OF MEMORY TO STORE NEW DEVICE'S");
	}

	while (i > 0 && registry[i - 1].id > id) {
		registry[i] = registry[i - 1];
		i--;
	}

	registry[i] = (device_registry_entry_t){id, type, dev_ptr};
	registry_count++;
	return id;
}

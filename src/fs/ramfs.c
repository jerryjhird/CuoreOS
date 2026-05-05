#include "ramfs.h"
#include "mem/mem.h"

void ramfs_init(ramfs_handle_t *handle, void *ptr) {
	handle->base = ptr;
	handle->header = (ramfs_header_t *)ptr;
	handle->entries = (ramfs_entry_t *)((uintptr_t)ptr + sizeof(ramfs_header_t));
}

ramfs_file_t ramfs_get_file(ramfs_handle_t *handle, const char *name) {
	for (uint32_t i = 0; i < handle->header->file_count; i++) {
		if (strcmp(handle->entries[i].name, name) == 0) {
			return (ramfs_file_t){
				.data = (void *)((uintptr_t)handle->base + handle->entries[i].offset),
				.size = handle->entries[i].size
			};
		}
	}
	return (ramfs_file_t){NULL, 0};
}

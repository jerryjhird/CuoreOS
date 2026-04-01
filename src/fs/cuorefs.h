#pragma once

#include <stdint.h>
#ifndef INCLUDE_AS_TOOL
#include "disk/partition.h"
void cuorefs_init(partition_t* part);
int cuorefs_format(partition_t *part, uint32_t max_files) ;

extern partition_t* glob_cuorefs_partition;

#endif

#define BLOCK_SIZE 4096
#define SECTOR_SIZE 512
#define FS_MAGIC 0x43554F52454653

// used by kernel at runtime

typedef struct {
	void* data;
	uint64_t size;
} cuorefs_file_t;

// included in the filesystem

typedef struct __attribute__((packed)) {
	char name[64];
	uint64_t block_start;
	uint64_t size_bytes;
	uint64_t reserved;
} cuorefs_entry_t;

typedef struct __attribute__((packed)) {
	uint64_t magic;
	uint32_t num_files;
	uint32_t table_blocks;
	uint8_t  bitmap[4080];
} cuorefs_header_t;

cuorefs_file_t* cuorefs_find_file(const char* name);
int cuorefs_add_file(const char *name, void *data, uint64_t size);
int cuorefs_delete_file(const char* name);
int cuorefs_wipe_file(const char* name); // secure delete

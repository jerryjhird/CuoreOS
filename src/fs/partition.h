#pragma once

#include "devicetypes.h"
#include <stdbool.h>

typedef struct {
	uint64_t start_lba;
	uint64_t sector_count;
	uint32_t partition_id; // index
	uint8_t type_id;	   // legacy MBR type (0x0C, etc)
	uint8_t guid[16];	   // for GPT systems (0 for MBR)
	kernel_dev_t *disk;	   // device this partition belongs to
	bool is_gpt;

	struct partition_s *next;
} partition_t;

typedef struct {
	uint8_t mbr_type;
	uint8_t guid[16];
	const char *name;
} partition_type_lookup_t;

void partition_register(partition_t *new_part);
const char *partition_get_name(partition_t *p);
#include "mbr.h"
#include "partition.h"
#include "mem/heap.h" // IWYU pragma: keep
#include "mem/mem.h" // IWYU pragma: keep
#include "partition.h"

typedef struct {
	uint8_t type;
	const char* name;
} partition_type_t;

uint8_t mbr_parse(kernel_dev_t* dev) {
	uint8_t sector_buffer[512];

	if (dev->read_sector(dev, 0, (uint16_t*)sector_buffer) != 0) {
		return 1;
	}

	mbr_t* mbr = (mbr_t*)sector_buffer;
	if (mbr->signature != 0xAA55) {
		return 1;
	}

	for (int i = 0; i < 4; i++) {
		if (mbr->partitions[i].partition_type == 0xEE) {
			return 2;
		}
	}

	for (int i = 0; i < 4; i++) {
		mbr_partition_t* p = &mbr->partitions[i];
		if (p->partition_type == 0) continue;

		partition_t* entry = (partition_t*)malloc(sizeof(partition_t));
		if (!entry) continue;

		entry->start_lba	= p->lba_start;
		entry->sector_count = p->lba_length;
		entry->type_id	  = p->partition_type;
		entry->disk		 = dev;
		entry->is_gpt	   = false;

		partition_register(entry);
	}

	return 0;
}

void mbr_install(kernel_dev_t* dev, uint8_t type_id) {
	uint8_t sector[512];
	memset(sector, 0, 512);

	uint8_t* partition_entry = &sector[446];

	partition_entry[0] = 0x80;
	partition_entry[1] = 0x00;
	partition_entry[2] = 0x01;
	partition_entry[3] = 0x01;

	partition_entry[4] = type_id ? type_id : 0x83;

	partition_entry[5] = 0x00;
	partition_entry[6] = 0x01;
	partition_entry[7] = 0x01;

	uint32_t start_lba = 2048;
	uint32_t total_sectors = 0xFFFE;

	*(uint32_t*)&partition_entry[8] = start_lba;
	*(uint32_t*)&partition_entry[12] = total_sectors;

	sector[510] = 0x55;
	sector[511] = 0xAA;

	dev->write_sector(dev, 0, (uint16_t*)sector);

	memset(sector, 0, 512);
	dev->write_sector(dev, 2048, (uint16_t*)sector);
}

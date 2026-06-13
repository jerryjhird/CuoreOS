#include "mbr.h"
#include "partition.h"
#include "mem/heap.h"
#include "mem/mem.h"
#include "mem/dmalloc.h"

uint8_t mbr_parse(kernel_disk_dev_t* dev) {
	dmalloc_ret_t sector_res = dmalloc32(512);
	if (!sector_res.virt) return 1;

	if (dev->read_sectors(dev, 0, 1, sector_res) != 0) {
		dmfree(sector_res.virt);
		return 1;
	}

	mbr_t* mbr = (mbr_t*)sector_res.virt;
	if (mbr->signature != 0xAA55) {
		dmfree(sector_res.virt);
		return 1;
	}

	for (int i = 0; i < 4; i++) {
		if (mbr->partitions[i].partition_type == 0xEE) {
			dmfree(sector_res.virt);
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

	dmfree(sector_res.virt);
	return 0;
}

void mbr_install(kernel_disk_dev_t *dev, uint8_t type_id) {
	dmalloc_ret_t sector_res = dmalloc32(512);
	if (!sector_res.virt) return;

	uint8_t* sector = (uint8_t*)sector_res.virt;
	memset(sector, 0, 512);

	uint8_t *partition_entry = &sector[446];

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

	memcpy(&partition_entry[8], &start_lba, sizeof(uint32_t));
	memcpy(&partition_entry[12], &total_sectors, sizeof(uint32_t));

	sector[510] = 0x55;
	sector[511] = 0xAA;

	dev->write_sectors(dev, 0, 1, sector_res);

	memset(sector, 0, 512);
	dev->write_sectors(dev, 2048, 1, sector_res);

	dmfree(sector_res.virt);
}

#include "gpt.h"
#include "devices.h"
#include "guid_list.h"
#include "partition.h"
#include "logbuf.h"
#include "mem/heap.h" // IWYU pragma: keep
#include "mem/mem.h" // IWYU pragma: keep
#include "crc32.h"

void gpt_parse(kernel_disk_dev_t* dev) {
	uint8_t buffer[512];

	if (dev->read_sector(dev, 1, (uint16_t*)buffer) != 0) return;
	gpt_header_t* header = (gpt_header_t*)buffer;

	if (memcmp(header->signature, "EFI PART", 8) != 0) return;

	uint32_t read_crc = header->header_crc32;
	header->header_crc32 = 0;

	if (crc32(header, header->header_size) != read_crc) {
		logbuf_write("[ GPT  ] Header CRC32 Mismatch! Aborting.\n");
		return;
	}
	header->header_crc32 = read_crc;

	uint32_t array_size = header->num_partition_entries * header->sizeof_partition_entry;
	uint8_t* array_buffer = (uint8_t*)malloc(array_size);

	if (!array_buffer) return;

	uint32_t sectors_to_read = array_size / 512;
	for (uint32_t s = 0; s < sectors_to_read; s++) {
		dev->read_sector(dev, header->partition_entry_lba + s,
						 (uint16_t*)(array_buffer + (s * 512)));
	}

	if (crc32(array_buffer, array_size) != header->partition_entry_array_crc32) {
		logbuf_write("[ GPT  ] Partition Entry Array CRC32 Mismatch!\n");
		free(array_buffer);
		return;
	}

	uint32_t entry_size = header->sizeof_partition_entry;

	for (uint32_t i = 0; i < header->num_partition_entries; i++) {
		gpt_entry_t* gpt_p = (gpt_entry_t*)&array_buffer[i * entry_size];

		if (memcmp(gpt_p->partition_type_guid, (uint8_t[])GUID_EMPTY, 16) == 0) {
			continue;
		}

		partition_t* entry = (partition_t*)malloc(sizeof(partition_t));
		if (!entry) break;

		entry->start_lba	= gpt_p->starting_lba;
		entry->sector_count = (gpt_p->ending_lba - gpt_p->starting_lba) + 1;
		entry->partition_id = i;
		entry->type_id	  = 0;
		entry->disk		 = dev;
		entry->is_gpt	   = true;
		memcpy(entry->guid, gpt_p->partition_type_guid, 16);

		partition_register(entry);
	}

	free(array_buffer);
}

void gpt_install(kernel_disk_dev_t* dev, const char* fs_name, uint8_t* type_guid) {
	uint64_t disk_sectors = dev->total_sectors;
	if (disk_sectors < 68) return;

	uint8_t sector[512];
	uint32_t array_crc;

	uint8_t* full_array = malloc(16384);
	memset(full_array, 0, 16384);
	gpt_entry_t* entry = (gpt_entry_t*)full_array;

	if (type_guid) {
		memcpy(entry->partition_type_guid, type_guid, 16);
	} else {
		memcpy(entry->partition_type_guid, (uint8_t[])GUID_LINUX_FILESYSTEM, 16);
	}

	memset(entry->unique_partition_guid, 0xAA, 16);
	entry->starting_lba = 2048;
	entry->ending_lba = disk_sectors - 34;

	if (fs_name) {
		for(int i = 0; fs_name[i] != '\0' && i < 36; i++) {
			entry->partition_name[i] = (uint16_t)fs_name[i];
		}
	}

	array_crc = crc32(full_array, 16384);

	memset(sector, 0, 512);
	sector[450] = 0xEE;
	*(uint32_t*)&sector[454] = 1;
	uint32_t mbr_sz = (disk_sectors > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)(disk_sectors - 1);
	*(uint32_t*)&sector[458] = mbr_sz;
	sector[510] = 0x55;
	sector[511] = 0xAA;
	dev->write_sector(dev, 0, (uint16_t*)sector);

	for (int i = 0; i < 32; i++) {
		dev->write_sector(dev, 2 + i, (uint16_t*)(full_array + (i * 512)));
	}

	for (int i = 0; i < 32; i++) {
		dev->write_sector(dev, (disk_sectors - 33) + i, (uint16_t*)(full_array + (i * 512)));
	}

	// primary header
	memset(sector, 0, 512);
	gpt_header_t* header = (gpt_header_t*)sector;
	memcpy(header->signature, "EFI PART", 8);
	header->revision = 0x00010000;
	header->header_size = 92;
	header->my_lba = 1;
	header->alternate_lba = disk_sectors - 1;
	header->first_usable_lba = 34;
	header->last_usable_lba = disk_sectors - 34;
	memset(header->disk_guid, 0xCC, 16);
	header->partition_entry_lba = 2;
	header->num_partition_entries = 128;
	header->sizeof_partition_entry = 128;
	header->partition_entry_array_crc32 = array_crc;

	header->header_crc32 = 0;
	header->header_crc32 = crc32(header, 92);
	dev->write_sector(dev, 1, (uint16_t*)sector);

	// backup header
	header->my_lba = disk_sectors - 1;
	header->alternate_lba = 1;
	header->partition_entry_lba = disk_sectors - 33;

	header->header_crc32 = 0;
	header->header_crc32 = crc32(header, 92);
	dev->write_sector(dev, disk_sectors - 1, (uint16_t*)sector);

	free(full_array);
}

#include "gpt.h"
#include "guid_list.h"
#include "partition.h"
#include "../logbuf.h"
#include "mem/heap.h" // IWYU pragma: keep
#include "mem/mem.h" // IWYU pragma: keep

void gpt_parse(kernel_dev_t* dev) {
	uint8_t buffer[512];
	if (dev->read_sector(1, (uint16_t*)buffer) != 0) return;

	gpt_header_t* header = (gpt_header_t*)buffer;
	if (memcmp(header->signature, "EFI PART", 8) != 0) return;

	uint32_t entry_size = header->sizeof_partition_entry;
	uint32_t entries_per_sector = 512 / entry_size;

	uint64_t cached_lba = 0;

	for (uint32_t i = 0; i < header->num_partition_entries; i++) {
		uint64_t sector_to_read = header->partition_entry_lba + (i / entries_per_sector);

		if (sector_to_read != cached_lba) {
			if (dev->read_sector(sector_to_read, (uint16_t*)buffer) != 0) break;
			cached_lba = sector_to_read;
		}

		gpt_entry_t* gpt_p = (gpt_entry_t*)&buffer[(i % entries_per_sector) * entry_size];

		if (memcmp(gpt_p->partition_type_guid, (uint8_t[])GUID_EMPTY, 16) == 0) continue;

		partition_t* entry = (partition_t*)malloc(sizeof(partition_t));
		entry->start_lba	= gpt_p->starting_lba;
		entry->sector_count = (gpt_p->ending_lba - gpt_p->starting_lba) + 1;
		entry->partition_id = i;
		entry->type_id	  = 0;
		entry->disk		 = dev;
		entry->is_gpt	   = true;
		memcpy(entry->guid, gpt_p->partition_type_guid, 16);

		partition_register(entry);

		logbuf_write("[ GPT  ] ");
		logbuf_write(partition_get_name(entry));
		logbuf_write(" added to partition table\n");
	}
}

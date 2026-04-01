#include "cuorefs.h"
#include "logbuf.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "mem/heap.h"
#include "fs/partition.h"
#include "drivers/ramfs.h"
#include <stdarg.h>

partition_t* glob_cuorefs_partition = NULL;

static void bitmap_set(uint8_t *bitmap, uint32_t bit) {
	bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_clear(uint8_t *bitmap, uint32_t bit) {
	bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int bitmap_test(uint8_t *bitmap, uint32_t bit) {
	return bitmap[bit / 8] & (1 << (bit % 8));
}

void cuorefs_init(partition_t* part) {
	uint8_t* buffer = malloc(BLOCK_SIZE);
	for (int i = 0; i < 8; i++) {
		if (part->disk->read_sector(part->start_lba + i, (uint16_t*)(buffer + (i * 512))) != 0) {
			free(buffer);
			return;
		}
	}

	cuorefs_header_t* header = (cuorefs_header_t*)buffer;
	if (header->magic == FS_MAGIC) {
		part->is_cuorefs = true;
		part->fs_metadata = buffer;
		logbuf_write("[  FS  ] CUORE-FS detected\n");
		glob_cuorefs_partition = part;
	} else {
		free(buffer);
	}
}

int cuorefs_format(partition_t *part, uint32_t max_files) {
	uint64_t table_bytes = (uint64_t)max_files * sizeof(cuorefs_entry_t);
	uint32_t table_blocks = (uint32_t)((table_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE);

	cuorefs_header_t *header = zalloc(BLOCK_SIZE);
	header->magic = FS_MAGIC;
	header->num_files = 0;
	header->table_blocks = table_blocks;

	bitmap_set(header->bitmap, 0);
	for (uint32_t i = 1; i <= table_blocks; i++) {
		bitmap_set(header->bitmap, i);
	}

	for (int i = 0; i < 8; i++) {
		part->disk->write_sector(part->start_lba + i, (uint16_t*)((uint8_t*)header + (i * SECTOR_SIZE)));
	}

	uint8_t *zero_block = zalloc(BLOCK_SIZE);
	for (uint32_t b = 0; b < table_blocks; b++) {
		uint64_t block_lba = part->start_lba + 8 + (b * 8);
		for (int s = 0; s < 8; s++) {
			part->disk->write_sector(block_lba + s, (uint16_t*)(zero_block + (s * SECTOR_SIZE)));
		}
	}

	if (part->fs_metadata) free(part->fs_metadata);
	part->fs_metadata = header;
	part->is_cuorefs = true;
	glob_cuorefs_partition = part;

	free(zero_block);
	return 0;
}

int cuorefs_add_file(const char *name, void *data, uint64_t size) {
	if (!glob_cuorefs_partition) return -1;
	kernel_dev_t *disk = glob_cuorefs_partition->disk;
	uint64_t part_lba = glob_cuorefs_partition->start_lba;
	cuorefs_header_t *header = (cuorefs_header_t*)glob_cuorefs_partition->fs_metadata;

	uint32_t needed_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	uint32_t start_block = 0;
	uint32_t contiguous_found = 0;
	int space_found = 0;

	uint32_t reserved_area = 1 + header->table_blocks;
	for (uint32_t i = reserved_area; i < 32640; i++) {
		if (!bitmap_test(header->bitmap, i)) {
			if (contiguous_found == 0) start_block = i;
			contiguous_found++;
			if (contiguous_found == needed_blocks) {
				space_found = 1;
				break;
			}
		} else {
			contiguous_found = 0;
		}
	}

	if (!space_found) return -1;

	uint32_t table_size = header->table_blocks * BLOCK_SIZE;
	cuorefs_entry_t *table = malloc(table_size);
	for(uint32_t s = 0; s < (header->table_blocks * 8); s++) {
		disk->read_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));
	}

	for (uint32_t i = start_block; i < start_block + needed_blocks; i++) {
		bitmap_set(header->bitmap, i);
	}

	cuorefs_entry_t *new_entry = &table[header->num_files];
	memset(new_entry, 0, sizeof(cuorefs_entry_t));
	strncpy(new_entry->name, name, 63);
	new_entry->size_bytes = size;
	new_entry->block_start = start_block;

	uint64_t file_lba = part_lba + (start_block * 8);
	uint64_t total_sectors = (size + 511) / 512;
	for (uint64_t s = 0; s < total_sectors; s++) {
		uint8_t bounce[512] = {0};
		uint64_t to_copy = (s == total_sectors - 1 && size % 512 != 0) ? (size % 512) : 512;
		memcpy(bounce, (uint8_t*)data + (s * 512), to_copy);
		disk->write_sector(file_lba + s, (uint16_t*)bounce);
	}

	header->num_files++;

	for(int i = 0; i < 8; i++)
		disk->write_sector(part_lba + i, (uint16_t*)((uint8_t*)header + (i * SECTOR_SIZE)));
	for(uint32_t s = 0; s < (header->table_blocks * 8); s++)
		disk->write_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));

	free(table);
	return 0;
}

int cuorefs_delete_file(const char* name) {
	if (!glob_cuorefs_partition || !glob_cuorefs_partition->fs_metadata) return -1;
	kernel_dev_t *disk = glob_cuorefs_partition->disk;
	uint64_t part_lba = glob_cuorefs_partition->start_lba;
	cuorefs_header_t *header = (cuorefs_header_t*)glob_cuorefs_partition->fs_metadata;

	uint32_t table_size = header->table_blocks * BLOCK_SIZE;
	cuorefs_entry_t *table = malloc(table_size);
	for (uint32_t s = 0; s < (header->table_blocks * 8); s++) {
		disk->read_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));
	}

	int found_idx = -1;
	for (uint32_t i = 0; i < header->num_files; i++) {
		if (strncmp(table[i].name, name, 63) == 0) {
			found_idx = i;
			break;
		}
	}

	if (found_idx == -1) {
		free(table);
		return -1;
	}

	uint32_t blocks_to_free = (table[found_idx].size_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
	for (uint32_t i = 0; i < blocks_to_free; i++) {
		bitmap_clear(header->bitmap, table[found_idx].block_start + i);
	}

	for (uint32_t i = found_idx; i < header->num_files - 1; i++) {
		table[i] = table[i + 1];
	}
	memset(&table[header->num_files - 1], 0, sizeof(cuorefs_entry_t));
	header->num_files--;

	for (int i = 0; i < 8; i++)
		disk->write_sector(part_lba + i, (uint16_t*)((uint8_t*)header + (i * SECTOR_SIZE)));
	for (uint32_t s = 0; s < (header->table_blocks * 8); s++)
		disk->write_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));

	free(table);
	return 0;
}

int cuorefs_wipe_file(const char* name) {
	if (!glob_cuorefs_partition || !glob_cuorefs_partition->fs_metadata) return -1;
	kernel_dev_t *disk = glob_cuorefs_partition->disk;
	uint64_t part_lba = glob_cuorefs_partition->start_lba;
	cuorefs_header_t *header = (cuorefs_header_t*)glob_cuorefs_partition->fs_metadata;

	uint32_t table_size = header->table_blocks * BLOCK_SIZE;
	cuorefs_entry_t *table = malloc(table_size);
	for (uint32_t s = 0; s < (header->table_blocks * 8); s++) {
		disk->read_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));
	}

	int found_idx = -1;
	for (uint32_t i = 0; i < header->num_files; i++) {
		if (strncmp(table[i].name, name, 63) == 0) {
			found_idx = i;
			break;
		}
	}

	if (found_idx == -1) {
		free(table);
		return -1;
	}

	uint64_t file_lba = part_lba + (table[found_idx].block_start * 8);
	uint64_t total_sectors = (table[found_idx].size_bytes + 511) / 512;
	uint8_t zero_buffer[512] = {0};
	for (uint64_t s = 0; s < total_sectors; s++) {
		disk->write_sector(file_lba + s, (uint16_t*)zero_buffer);
	}

	uint32_t blocks_to_free = (table[found_idx].size_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
	for (uint32_t i = 0; i < blocks_to_free; i++) {
		bitmap_clear(header->bitmap, table[found_idx].block_start + i);
	}

	for (uint32_t i = found_idx; i < header->num_files - 1; i++) {
		table[i] = table[i + 1];
	}
	memset(&table[header->num_files - 1], 0, sizeof(cuorefs_entry_t));
	header->num_files--;

	for (int i = 0; i < 8; i++)
		disk->write_sector(part_lba + i, (uint16_t*)((uint8_t*)header + (i * SECTOR_SIZE)));
	for (uint32_t s = 0; s < (header->table_blocks * 8); s++)
		disk->write_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));

	free(table);
	return 0;
}

cuorefs_file_t* cuorefs_find_file(const char* name) {
	if (!glob_cuorefs_partition || !glob_cuorefs_partition->fs_metadata) return NULL;
	kernel_dev_t *disk = glob_cuorefs_partition->disk;
	uint64_t part_lba = glob_cuorefs_partition->start_lba;
	cuorefs_header_t *header = (cuorefs_header_t*)glob_cuorefs_partition->fs_metadata;

	uint32_t table_size = header->table_blocks * BLOCK_SIZE;
	cuorefs_entry_t *table = malloc(table_size);
	for (uint32_t s = 0; s < (header->table_blocks * 8); s++) {
		disk->read_sector(part_lba + 8 + s, (uint16_t*)((uint8_t*)table + (s * SECTOR_SIZE)));
	}

	int found_idx = -1;
	for (uint32_t i = 0; i < header->num_files; i++) {
		if (strncmp(table[i].name, name, 63) == 0) {
			found_idx = i;
			break;
		}
	}

	if (found_idx == -1) {
		free(table);
		return NULL;
	}

	cuorefs_file_t* file = malloc(sizeof(cuorefs_file_t));
	file->size = table[found_idx].size_bytes;
	file->data = malloc(file->size);

	uint64_t file_lba = part_lba + (table[found_idx].block_start * 8);
	uint32_t total_sectors = (file->size + 511) / 512;
	for (uint32_t s = 0; s < total_sectors; s++) {
		uint8_t bounce[512];
		disk->read_sector(file_lba + s, (uint16_t*)bounce);
		uint32_t to_copy = (s == total_sectors - 1 && file->size % 512 != 0) ? (file->size % 512) : 512;
		memcpy((uint8_t*)file->data + (s * 512), bounce, to_copy);
	}

	free(table);
	return file;
}

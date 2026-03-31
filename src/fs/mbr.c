#include "mbr.h"
#include "partition.h"
#include "../logbuf.h"
#include "mem/heap.h"
#include "partition.h"

typedef struct {
    uint8_t type;
    const char* name;
} partition_type_t;

static const partition_type_t partition_types[] = {
    {0x00, "Empty"},
    {0x01, "FAT12"},
    {0x04, "FAT16 (Small)"},
    {0x06, "FAT16"},
    {0x07, "NTFS/exFAT"},
    {0x0B, "FAT32 (CHS)"},
    {0x0C, "FAT32 (LBA)"},
    {0x0E, "FAT16 (LBA)"},
    {0x82, "Linux Swap"},
    {0x83, "Linux Native"},
    {0xEE, "GPT Protective"},
    {0xEF, "EFI System Partition"},
    {0, NULL}
};

const char* mbr_get_fs_name(uint8_t type) {
    for (int i = 0; partition_types[i].name != NULL; i++) {
        if (partition_types[i].type == type) {
            return partition_types[i].name;
        }
    }
    return "Unknown";
}

void mbr_parse(kernel_dev_t* dev, uint8_t* sector_buffer) {
    mbr_t* mbr = (mbr_t*)sector_buffer;
    if (mbr->signature != 0xAA55) return;

    for (int i = 0; i < 4; i++) {
        mbr_partition_t* p = &mbr->partitions[i];
        if (p->partition_type == 0) continue;

        partition_t* entry = (partition_t*)malloc(sizeof(partition_t));

        entry->start_lba    = p->lba_start;
        entry->sector_count = p->lba_length;
        entry->type_id      = p->partition_type;
        entry->disk         = dev;

        partition_register(entry);
        
        logbuf_write("[ MBR  ] ");
        logbuf_write(mbr_get_fs_name(p->partition_type));
        logbuf_write(" Detected and successfully registered\n");
    }
}
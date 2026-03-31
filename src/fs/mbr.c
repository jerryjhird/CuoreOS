#include "mbr.h"
#include "partition.h"
#include "../logbuf.h"
#include "mem/heap.h" // IWYU pragma: keep
#include "partition.h"

typedef struct {
    uint8_t type;
    const char* name;
} partition_type_t;

uint8_t mbr_parse(kernel_dev_t* dev, uint8_t* sector_buffer) {
    mbr_t* mbr = (mbr_t*)sector_buffer;
    if (mbr->signature != 0xAA55) return 1; // invalid MBR signature

    for (int i = 0; i < 4; i++) {
        if (mbr->partitions[i].partition_type == 0xEE) {
            return 2; // GPT protective MBR 
        }
    }

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
        logbuf_write(partition_get_name(entry));
        logbuf_write(" added to partition table\n");
    }

    return 0; // success
}
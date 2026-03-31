#pragma once

#include <stdint.h>
#include "devicetypes.h"

typedef struct {
    uint8_t  attributes; // 0x80 = bootable
    uint8_t  chs_start[3];
    uint8_t  partition_type; // 0xEE = GPT Protective, etc
    uint8_t  chs_end[3];
    uint32_t lba_start; // first sector of partition
    uint32_t lba_length; // size in sectors
} __attribute__((packed)) mbr_partition_t;

typedef struct {
    uint8_t         bootstrap[446]; // code area
    mbr_partition_t partitions[4]; // 4 primary partitions
    uint16_t        signature; // 0xAA55
} __attribute__((packed)) mbr_t;

uint8_t mbr_parse(kernel_dev_t* dev, uint8_t* sector_buffer);
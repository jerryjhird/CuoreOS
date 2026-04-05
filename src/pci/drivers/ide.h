#pragma once

#include "pci/pci.h"

typedef struct {
	uint16_t port_base;
	uint64_t total_sectors;
	char model[41];
} ide_ctx_t;

#define IDE_REG_DATA 0
#define IDE_REG_FEATURES 1
#define IDE_REG_SECTOR_CNT 2
#define IDE_REG_LBA_LOW	3
#define IDE_REG_LBA_MID 4
#define IDE_REG_LBA_HIGH 5
#define IDE_REG_DRIVE_SEL 6
#define IDE_REG_COMMAND 7
#define IDE_REG_STATUS 7
#define IDE_CMD_READ 0x20
#define IDE_CMD_WRITE 0x30
#define IDE_STATUS_BSY 0x80
#define IDE_STATUS_DRQ 0x08
#define IDE_SEL_LBA 0x40
#define IDE_SEL_MASTER 0xA0
#define IDE_CMD_IDENTIFY 0xEC

void ide_init(pci_dev_t dev);

#include <stdint.h>
typedef int dummy0; // satisfy ISO C / -Wpedantic

// driver for IDE disks. only supports PMIO
#include "Config.h"

#ifdef KERNEL_MOD_IDE_ENABLED
#include "pci/pci.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "mem/dmalloc.h"
#include "_time.h"
#include "ide.h"
#include "mem/mem.h"
#include "disk/ata.h"
#include "devices/registry.h"

static uint8_t ide_read_sectors(kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer) {
	uint16_t base = dev->port_base;
	uint8_t count8 = (uint8_t)count;
	uint16_t* ptr = (uint16_t*)buffer.virt;

	if (count8 == 0) return 0;

	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));
	outb(base + IDE_REG_SECTOR_CNT, count8);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));
	outb(base + IDE_REG_COMMAND,   IDE_CMD_READ);

	for (uint32_t i = 0; i < count8; i++) {
		while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
		while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));

		if (inb(base + IDE_REG_STATUS) & 0x21) return 1;
		insw(base + IDE_REG_DATA, ptr + (i * 256), 256);
	}

	return 0;
}

static uint8_t ide_write_sectors(kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer) {
	uint16_t base = dev->port_base;
	uint8_t count8 = (uint8_t)count;
	uint16_t* ptr = (uint16_t*)buffer.virt;

	if (count8 == 0) return 0;

	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));
	outb(base + IDE_REG_SECTOR_CNT, count8);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));
	outb(base + IDE_REG_COMMAND,   IDE_CMD_WRITE);

	for (uint32_t i = 0; i < count8; i++) {
		while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
		while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));
		if (inb(base + IDE_REG_STATUS) & 0x21) return 1;
		outsw(base + IDE_REG_DATA, ptr + (i * 256), 256);
		usleep(1);
	}

	outb(base + IDE_REG_COMMAND, 0xE7);
	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);

	return 0;
}

pci_driver_status ide_init(pci_dev_t pdev) {
	uint16_t base = (pdev.bars[0].is_io && pdev.bars[0].base != 0)
					? (uint16_t)pdev.bars[0].base
					: 0x1F0;

	uint8_t status = inb(base + IDE_REG_STATUS);
	if (status == 0xFF) return CARD_NOT_PRESENT;

	outb(base + IDE_REG_DRIVE_SEL, 0xA0);
	usleep(1);

	outb(base + IDE_REG_SECTOR_CNT, 0);
	outb(base + IDE_REG_LBA_LOW, 0);
	outb(base + IDE_REG_LBA_MID, 0);
	outb(base + IDE_REG_LBA_HIGH, 0);
	outb(base + IDE_REG_COMMAND, IDE_CMD_IDENTIFY);

	status = inb(base + IDE_REG_STATUS);
	if (status == 0) return CARD_HARDWARE_STALLING;

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);

	if (inb(base + IDE_REG_LBA_MID) == 0x14 && inb(base + IDE_REG_LBA_HIGH) == 0xEB) {
		return DRIVER_UNSUPPORTED_CARD;
	}

	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ)) {
		if (inb(base + IDE_REG_STATUS) & 0x01) return CARD_HARDWARE_FAILURE;
	}

	kernel_disk_dev_t* dev = zalloc(sizeof(kernel_disk_dev_t));

	memset(dev, 0, sizeof(kernel_disk_dev_t));

	uint16_t data[256];
	insw(base + IDE_REG_DATA, data, 256);

	ata_identity_t identity = ata_identify(data);
	memcpy(dev->model, identity.model, sizeof(dev->model));
	dev->total_sectors = identity.total_sectors;

	dev->port_base = base;
	dev->read_sectors = ide_read_sectors;
	dev->write_sectors = ide_write_sectors;

	device_register(DISK_DEV, dev);

	logbuf_ok("[ IDE  ] Found \"%s\" (%llu MiB)\n",  dev->model, (unsigned long long)(dev->total_sectors / 2048));

	return DRIVER_OK;
}

#endif

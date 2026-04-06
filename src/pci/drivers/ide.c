#include "devices.h"
#include "devices.h"
#include "pci/pci.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "disk/diskinit.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "stdio.h"
#include "ide.h"

static uint8_t ide_read_sector(kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer) {
	uint16_t base = dev->port_base;

	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));
	outb(base + IDE_REG_SECTOR_CNT, 1);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));
	outb(base + IDE_REG_COMMAND,   IDE_CMD_READ);

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));

	if (inb(base + IDE_REG_STATUS) & 0x21) return 1;

	insw(base + IDE_REG_DATA, buffer, 256);
	return 0;
}

static uint8_t ide_write_sector(kernel_disk_dev_t* dev, uint32_t lba, uint16_t* buffer) {
	uint16_t base = dev->port_base;

	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));
	outb(base + IDE_REG_SECTOR_CNT, 1);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));
	outb(base + IDE_REG_COMMAND,   IDE_CMD_WRITE);

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));

	if (inb(base + IDE_REG_STATUS) & 0x21) return 1;

	outsw(base + IDE_REG_DATA, buffer, 256);
	outb(base + IDE_REG_COMMAND, 0xE7); // Cache Flush
	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);

	return 0;
}

static void ide_identify_drive(kernel_disk_dev_t* dev) {
	uint16_t base = dev->port_base;
	uint16_t temp_buffer[256];

	outb(base + IDE_REG_DRIVE_SEL, IDE_SEL_MASTER);
	outb(base + IDE_REG_COMMAND, IDE_CMD_IDENTIFY);

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
	uint8_t status = inb(base + IDE_REG_STATUS);
	if (status == 0 || (status & 0x01)) return;

	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));
	insw(base + IDE_REG_DATA, temp_buffer, 256);

	// get total sectors
	if (temp_buffer[83] & (1 << 10)) {
		memcpy(&dev->total_sectors, &temp_buffer[100], sizeof(uint64_t));
	} else {
		uint32_t lba28_sectors = 0;
		memcpy(&lba28_sectors, &temp_buffer[60], sizeof(uint32_t));
		dev->total_sectors = (uint64_t)lba28_sectors;
	}

	// get model name
	char* model_raw = (char*)&temp_buffer[27];
	for (int i = 0; i < 40; i++) {
		dev->model[i] = model_raw[i];
	}
	dev->model[40] = '\0';
	byteswap_str(dev->model, 40);

	for (int i = 39; i >= 0 && dev->model[i] == ' '; i--) {
		dev->model[i] = '\0';
	}
}

kernel_disk_dev_t ide_output_dev = {0};

void ide_init(pci_dev_t pdev) {
	uint16_t base;

	if (pdev.bars[0].is_io && pdev.bars[0].base != 0) {
		base = (uint16_t)pdev.bars[0].base;
	} else {
		base = 0x1F0;
	}

	outb(base + IDE_REG_DRIVE_SEL, IDE_SEL_MASTER);
	uint8_t status = inb(base + IDE_REG_STATUS);
	if (status == 0xFF || status == 0x00) {
		return;
	}

	ide_output_dev.port_base = base;

	ide_identify_drive(&ide_output_dev);

	ide_output_dev.read_sector = ide_read_sector;
	ide_output_dev.write_sector = ide_write_sector;

	logbuf_write("+ [ IDE  ] Initialized controller at: ");
	logbuf_puthex(base);
	logbuf_write("\n");

	disk_devices[disk_devices_c++] = &ide_output_dev;
	generic_disk_init(&ide_output_dev);
}

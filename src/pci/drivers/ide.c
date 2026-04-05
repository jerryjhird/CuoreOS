#include "devicetypes.h"
#include "devices.h"
#include "pci/pci.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "disk/diskinit.h"
#include "mem/pma.h"
#include "stdio.h"
#include "ide.h"

static uint8_t ide_read_sector(kernel_dev_t* dev, uint32_t lba, uint16_t* buffer) {
	ide_ctx_t* ctx = (ide_ctx_t*)dev->ctx;
	uint16_t base = ctx->port_base;

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

static uint8_t ide_write_sector(kernel_dev_t* dev, uint32_t lba, uint16_t* buffer) {
	ide_ctx_t* ctx = (ide_ctx_t*)dev->ctx;
	uint16_t base = ctx->port_base;

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

void ide_identify_drive(ide_ctx_t* ide_ctx) {
	uint16_t base = ide_ctx->port_base;
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
		ide_ctx->total_sectors = *(uint64_t*)&temp_buffer[100];
	} else {
		ide_ctx->total_sectors = *(uint32_t*)&temp_buffer[60];
	}

	// get model name
	char* model_raw = (char*)&temp_buffer[27];
	for (int i = 0; i < 40; i++) {
		ide_ctx->model[i] = model_raw[i];
	}
	ide_ctx->model[40] = '\0';
	byteswap_str(ide_ctx->model, 40);

	for (int i = 39; i >= 0 && ide_ctx->model[i] == ' '; i--) {
		ide_ctx->model[i] = '\0';
	}
}

SETUP_OUTPUT_DEVICE(ide_output_dev, CAP_IS_DISK, NULL, ide_read_sector, ide_write_sector);

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

	ide_ctx_t* ctx = (ide_ctx_t*)pma_alloc_page();
	ctx->port_base = base;

	ide_identify_drive(ctx);

	kernel_dev_t* dev = (kernel_dev_t*)pma_alloc_page();
	dev->DevCAP = CAP_IS_DISK;
	dev->ctx = ctx;
	dev->total_sectors = ctx->total_sectors;
	dev->read_sector = ide_read_sector;
	dev->write_sector = ide_write_sector;

	logbuf_write("+ [ IDE  ] Initialized controller at: ");
	logbuf_puthex(base);
	logbuf_write("\n");

	REGISTER_OUTPUT_DEVICE(dev, output_devices, output_devices_c);
	generic_disk_init(dev);
}

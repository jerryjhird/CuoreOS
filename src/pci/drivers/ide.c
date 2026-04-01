#include "devicetypes.h"
#include "devices.h"
#include "pci/pci.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "disk/diskinit.h" // for the software side of the disk initalization

#define IDE_REG_DATA		  0
#define IDE_REG_FEATURES	  1
#define IDE_REG_SECTOR_CNT	2
#define IDE_REG_LBA_LOW	   3
#define IDE_REG_LBA_MID	   4
#define IDE_REG_LBA_HIGH	  5
#define IDE_REG_DRIVE_SEL	 6
#define IDE_REG_COMMAND	   7
#define IDE_REG_STATUS		7

// command and status Bits
#define IDE_CMD_READ		  0x20
#define IDE_CMD_WRITE		 0x30
#define IDE_STATUS_BSY		0x80
#define IDE_STATUS_DRQ		0x08

// drive selection
#define IDE_SEL_LBA		   0x40
#define IDE_SEL_MASTER		0xA0

static uint16_t ide_primary_base = 0;
kernel_dev_t ide_output_dev;

uint8_t ide_read_sector(uint16_t base, uint32_t lba, uint16_t* buffer) {
	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));

	outb(base + IDE_REG_SECTOR_CNT, 1);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));

	outb(base + IDE_REG_COMMAND,   IDE_CMD_READ);

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));

	uint8_t status = inb(base + IDE_REG_STATUS);
	if (status & 0x21) return 1;

	insw(base + IDE_REG_DATA, buffer, 256);
	return 0;
}

uint8_t ide_write_sector(uint16_t base, uint32_t lba, uint16_t* buffer) {
	outb(base + IDE_REG_DRIVE_SEL, (IDE_SEL_MASTER | IDE_SEL_LBA) | ((lba >> 24) & 0x0F));

	outb(base + IDE_REG_SECTOR_CNT, 1);
	outb(base + IDE_REG_LBA_LOW,   (uint8_t)lba);
	outb(base + IDE_REG_LBA_MID,   (uint8_t)(lba >> 8));
	outb(base + IDE_REG_LBA_HIGH,  (uint8_t)(lba >> 16));

	outb(base + IDE_REG_COMMAND,   IDE_CMD_WRITE);

	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);
	while (!(inb(base + IDE_REG_STATUS) & IDE_STATUS_DRQ));

	uint8_t status = inb(base + IDE_REG_STATUS);
	if (status & 0x21) return 1;

	outsw(base + IDE_REG_DATA, buffer, 256);

	outb(base + IDE_REG_COMMAND, 0xE7);
	while (inb(base + IDE_REG_STATUS) & IDE_STATUS_BSY);

	return 0;
}

static uint8_t ide_dev_read(uint32_t lba, uint16_t* buffer) {return ide_read_sector(ide_primary_base, lba, buffer);}
static uint8_t ide_dev_write(uint32_t lba, uint16_t* buffer) {return ide_write_sector(ide_primary_base, lba, buffer);}

SETUP_OUTPUT_DEVICE(ide_output_dev, CAP_IS_DISK, NULL, ide_dev_read, ide_dev_write);

void ide_init(pci_dev_t dev) {
	if (dev.bars[0].is_io && dev.bars[0].base != 0) {
		ide_primary_base = (uint16_t)dev.bars[0].base;
	} else {
		ide_primary_base = 0x1F0;
	}

	logbuf_write("+ [ IDE  ] Controller discovered at IO port: ");
	logbuf_puthex(ide_primary_base);

	outb(ide_primary_base + IDE_REG_DRIVE_SEL, IDE_SEL_MASTER);

	uint8_t status = inb(ide_primary_base + IDE_REG_STATUS);
	if (status == 0xFF || status == 0x00) {
		logbuf_write("+ [ IDE  ] No drive connected to this controller.\n\n");
		return;
	} else {
		logbuf_write("\n\n");
	}

	REGISTER_OUTPUT_DEVICE(&ide_output_dev, output_devices, output_devices_c);
	generic_disk_init(&ide_output_dev);
}

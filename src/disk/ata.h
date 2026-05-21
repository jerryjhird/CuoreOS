#include <stdint.h>
#include <stdbool.h>

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_FLUSH_CACHE 0xE7

typedef struct {
	// basic info
	uint16_t config; // word 0: general configuration
	uint64_t total_sectors; // words 60-61 (LBA28) or 100-103 (LBA48)
	char model[41]; // words 27-46
	char serial[21]; // words 10-19
	char firmware[9]; // words 23-26

	// capabilities
	uint16_t capabilities; // word 49
	uint16_t field_validity; // word 53
	uint32_t pio_modes; // word 64
	uint32_t dma_modes; // word 63 (Multiword) / word 88 (Ultra DMA)

	// command set support
	uint16_t command_sets_82; // word 82: supported command sets
	uint16_t command_sets_83; // word 83: features/commands supported (LBA48, etc)

	uint16_t logical_sector_size; // word 106 (bits 0-11)
} ata_identity_t;

ata_identity_t ata_identify(const uint16_t* id_data);

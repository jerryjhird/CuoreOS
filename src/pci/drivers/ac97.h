#pragma once

#include "pci/pci.h"
#include "devices.h"

#define AC97_MIXER_RESET 0x00
#define AC97_MASTER_VOLUME 0x02
#define AC97_PCM_VOLUME 0x18
#define AC97_VENDOR_ID1 0x7C
#define AC97_VENDOR_ID2 0x7E

// register offsets for nambars
#define AC97_PCM_OUT_BDBAR 0x10
#define AC97_PCM_OUT_CIV 0x14
#define AC97_PCM_OUT_LVI 0x15
#define AC97_PCM_OUT_SR 0x16
#define AC97_PCM_OUT_CR 0x1B
#define AC97_GLOB_CNT 0x2C
#define AC97_GLOB_STA 0x30

// PCM Out Engine
#define AC97_PO_BDBAR 0x10  // buffer descriptor base address
#define AC97_PO_CIV 0x14  // current index value
#define AC97_PO_LVI 0x15  // last valid index
#define AC97_PO_SR 0x16  // status register
#define AC97_PO_PICB 0x18  // position in current Buffer
#define AC97_PO_CR 0x1B  // control register

typedef struct {
	uint32_t addr; // physical address of PCM data
	uint16_t length; // length in samples (frames)
	uint16_t flags; // bit 15: IOC (Interrupt on completion), bit 14: BUP (end of list)
} __attribute__((packed)) ac97_bdl_entry_t;

typedef struct {
	uint32_t nambar;
	uint32_t nabmbar;
	ac97_bdl_entry_t* bdl;
	uintptr_t bdl_phys;
} ac97_state_t;

void ac97_init(pci_dev_t dev);

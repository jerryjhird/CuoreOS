#pragma once

#include "pci/pci.h"

#define SATA_SIG_ATA	0x00000101

#define HBA_PxIS_IFS (1 << 27)
#define HBA_PxIS_HBFS (1 << 29)
#define HBA_PxIS_TFES (1 << 30)

#define HBA_PxCMD_ST	0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR	0x4000
#define HBA_PxCMD_CR	0x8000

typedef volatile struct {
	uint32_t clb, clbu, fb, fbu, is, ie, cmd, rsv0, tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs;
	uint32_t rsv1[11];
	uint32_t vendor[4];
} hba_port_t;

typedef volatile struct {
	uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
	uint8_t  rsv[0xA0 - 0x2C];
	uint8_t  vendor[0x100 - 0xA0];
	hba_port_t ports[32];
} hba_mem_t;

typedef struct {
	uint8_t  cfl:5, a:1, w:1, p:1, r:1, b:1, c:1, rsv0:1, pmp:4;
	uint16_t prdtl;
	volatile uint32_t prdbc;
	uint32_t ctba, ctbau;
	uint32_t rsv1[4];
} hba_cmd_header_t;

typedef struct {
	uint32_t dba, dbau, rsv0;
	uint32_t dbc:22, rsv1:9, i:1;
} hba_prdt_entry_t;

typedef struct {
	uint8_t  cfis[64];
	uint8_t  acmd[16];
	uint8_t  rsv[48];
	hba_prdt_entry_t prdt[1];
} hba_cmd_tbl_t;

typedef struct {
	hba_port_t* port;
	uint64_t* pml4;
} ahci_driver_state_t;

pci_driver_status ahci_init(pci_dev_t pdev);

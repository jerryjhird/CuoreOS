typedef int dummy0;

// driver for SATA disks. only supports MMIO
#include "Config.h"

#ifdef KERNEL_MOD_AHCI_ENABLED
#include "ahci.h"

#include "pci/pci.h"
#include "mem/heap.h"
#include "logbuf.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "mem/paging.h"
#include "mem/dmalloc.h"
#include "devices.h"
#include <stdint.h>
#include <string.h>
#include "_time.h"

static void ahci_stop_cmd(hba_port_t *port) {
	port->cmd &= ~HBA_PxCMD_ST;
	port->cmd &= ~HBA_PxCMD_FRE;
	while ((port->cmd & HBA_PxCMD_CR) || (port->cmd & HBA_PxCMD_FR));
}

static void ahci_start_cmd(hba_port_t *port) {
	while (port->cmd & HBA_PxCMD_CR);
	port->cmd |= HBA_PxCMD_FRE;
	port->cmd |= HBA_PxCMD_ST;
}

static uint8_t ahci_exec_cmd(kernel_disk_dev_t* dev, uint32_t lba, dmalloc_ret_t buffer, uint16_t count, uint8_t ata_cmd, bool write) {
	ahci_driver_state_t* priv = (ahci_driver_state_t*)dev->private_data;
	hba_port_t* port = priv->port;

	uint32_t slots = (port->sact | port->ci);
	int slot = -1;
	for (int i = 0; i < 32; i++) { if (!(slots & (1 << i))) { slot = i; break; } }
	if (slot == -1) return 1;

	uintptr_t phys_buffer = buffer.phys;
	if (!phys_buffer) return 1;

	uintptr_t clb_phys = ((uintptr_t)port->clbu << 32) | port->clb;
	hba_cmd_header_t* head = (hba_cmd_header_t*)(clb_phys + hhdm_offset) + slot;
	head->cfl = 5;
	head->w = write ? 1 : 0;
	head->prdtl = 1;

	uintptr_t ctba_phys = ((uintptr_t)head->ctbau << 32) | head->ctba;
	hba_cmd_tbl_t* tbl = (hba_cmd_tbl_t*)(ctba_phys + hhdm_offset);
	memset(tbl, 0, sizeof(hba_cmd_tbl_t));

	tbl->prdt[0].dba = (uint32_t)phys_buffer;
	tbl->prdt[0].dbau = (uint32_t)(phys_buffer >> 32);
	tbl->prdt[0].dbc = (count * 512) - 1;
	tbl->prdt[0].i = 1; // IOC

	tbl->cfis[0] = 0x27;
	tbl->cfis[1] = 0x80;
	tbl->cfis[2] = ata_cmd;

	if (ata_cmd == 0xEC) {
		tbl->cfis[12] = 1;
	} else {
		uint64_t lba64 = (uint64_t)lba;

		tbl->cfis[4] = (uint8_t)lba64;
		tbl->cfis[5] = (uint8_t)(lba64 >> 8);
		tbl->cfis[6] = (uint8_t)(lba64 >> 16);
		tbl->cfis[7] = 1 << 6;

		tbl->cfis[8] = (uint8_t)(lba64 >> 24);
		tbl->cfis[9] = (uint8_t)(lba64 >> 32);
		tbl->cfis[10] = (uint8_t)(lba64 >> 40);

		tbl->cfis[12] = (uint8_t)(count & 0xFF);
		tbl->cfis[13] = (uint8_t)((count >> 8) & 0xFF);
	}

	while (port->tfd & (0x80 | 0x08));
	port->ci = (1 << slot);

	while (1) {
		if (!(port->ci & (1 << slot))) break;
		if (port->is & (HBA_PxIS_TFES | HBA_PxIS_HBFS | HBA_PxIS_IFS)) return 1;
	}
	return 0;
}

static uint8_t ahci_read(kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer) {
	return ahci_exec_cmd(dev, lba, buffer, (uint16_t)count, 0x25, false);
}

static uint8_t ahci_write(kernel_disk_dev_t* dev, uint32_t lba, uint64_t count, dmalloc_ret_t buffer) {
	return ahci_exec_cmd(dev, lba, buffer, (uint16_t)count, 0x35, true);
}

void ahci_init(pci_dev_t pdev) {
	hba_mem_t* abar = (hba_mem_t*)(pdev.bars[5].base + hhdm_offset);

	if (abar->cap2 & 0x1) {
		abar->bohc |= (1 << 1); // request ownership
		while (abar->bohc & (1 << 0));
		msleep(25);
	}

	abar->ghc |= (1 << 31);

	uint32_t pi = abar->pi;
	for (int i = 0; i < 32; i++) {
		if (!(pi & (1 << i))) continue;

		hba_port_t* port = &abar->ports[i];
		if ((port->ssts & 0x0F) != 3 || port->sig != SATA_SIG_ATA) continue;

		port->is = port->is;
		port->serr = port->serr;

		ahci_stop_cmd(port);

		uintptr_t clb_p = pma_alloc_pages(1);
		memset((void*)(clb_p + hhdm_offset), 0, 4096);
		port->clb = (uint32_t)clb_p;
		port->clbu = (uint32_t)(clb_p >> 32);

		uintptr_t fb_p = pma_alloc_pages(1);
		memset((void*)(fb_p + hhdm_offset), 0, 4096);
		port->fb = (uint32_t)fb_p;
		port->fbu = (uint32_t)(fb_p >> 32);

		hba_cmd_header_t* head = (hba_cmd_header_t*)(clb_p + hhdm_offset);
		for (int j = 0; j < 32; j++) {
			uintptr_t ct_p = pma_alloc_pages(1);
			memset((void*)(ct_p + hhdm_offset), 0, 4096);
			head[j].ctba = (uint32_t)ct_p;
			head[j].ctbau = (uint32_t)(ct_p >> 32);
		}

		ahci_start_cmd(port);

		kernel_disk_dev_t* ddev = malloc(sizeof(kernel_disk_dev_t));
		ahci_driver_state_t* priv = malloc(sizeof(ahci_driver_state_t));
		priv->port = port;

		uintptr_t cr3;
		__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

		priv->pml4 = (uint64_t*)((cr3 & ~0xFFFULL) + hhdm_offset);

		ddev->private_data = priv;
		ddev->read_sectors = ahci_read;
		ddev->write_sectors = ahci_write;

		dmalloc_ret_t id_res = dmalloc32(512);
		if (id_res.virt) {
			memset((void*)id_res.virt, 0, 512);
			uint16_t* id_data = (uint16_t*)id_res.virt;

			if (ahci_exec_cmd(ddev, 0, id_res, 1, 0xEC, false) == 0) {
				for (int k = 0; k < 20; k++) {
					ddev->model[k * 2]   = (char)(id_data[27 + k] >> 8);
					ddev->model[k * 2 + 1] = (char)(id_data[27 + k] & 0xFF);
				}
				ddev->model[40] = '\0';

				for (int k = 39; k > 0 && ddev->model[k] == ' '; k--) ddev->model[k] = '\0';

				uint64_t lba48_sectors = 0;
				memcpy(&lba48_sectors, &id_data[100], sizeof(uint64_t));

				uint32_t lba28_sectors = 0;
				memcpy(&lba28_sectors, &id_data[60], sizeof(uint32_t));

				ddev->total_sectors = lba48_sectors;
				if (ddev->total_sectors == 0) {
					ddev->total_sectors = lba28_sectors;
				}
			} else {
				memcpy(ddev->model, "SATA DRIVE(Unknown Model)", 26);
			}
			dmfree(id_res.virt);
		}

		logbuf_write("[ SATA ] Initialized ");
		logbuf_write(ddev->model);
		logbuf_write("\n");

		disk_devices[disk_devices_c++] = ddev;
	}
}

#endif

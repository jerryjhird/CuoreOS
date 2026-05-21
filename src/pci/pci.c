#include "pci.h"
#include "cpu/io.h"
#include "mem/heap.h"
#include "logbuf.h"
#include <stddef.h>
#include "acpi/mcfg.h"

extern pci_driver_entry_t pci_discovery_table[];

//
// 32 bit access
//

static bool pci_check_driver_capabilities(uint32_t required_caps) {
	if ((required_caps & PCI_CAP_ECAM) && !mcfg_is_initialized) {
		return false;
	}

	// will add other checks here when i add other features

	return true;
}

uint32_t pci_read(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
	// try PCIe ECAM first
	void* addr = mcfg_get_device_addr(0, (uint8_t)bus, (uint8_t)slot, (uint8_t)func);
	if (addr) {
		return *(volatile uint32_t*)((uintptr_t)addr + (offset & 0xfff));
	}

	// fallback to port IO
	uint32_t address = (uint32_t)((((uint32_t)bus) << 16) |
					   (((uint32_t)slot) << 11) |
					   (((uint32_t)func) << 8) |
					   (offset & 0xfc) | 0x80000000);
	outl(PCI_CONFIG_ADDRESS, address);
	return inl(PCI_CONFIG_DATA);
}

void pci_write(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t data) {
	// try PCIe ECAM first
	void* addr = mcfg_get_device_addr(0, (uint8_t)bus, (uint8_t)slot, (uint8_t)func);
	if (addr) {
		*(volatile uint32_t*)((uintptr_t)addr + (offset & 0xfff)) = data;
		return;
	}

	// fallback to port IO
	uint32_t address = (uint32_t)((((uint32_t)bus) << 16) |
					   (((uint32_t)slot) << 11) |
					   (((uint32_t)func) << 8) |
					   (offset & 0xfc) | 0x80000000);
	outl(PCI_CONFIG_ADDRESS, address);
	outl(PCI_CONFIG_DATA, data);
}

//
// 8 bit access
//

uint8_t pci_read_byte(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
	void* addr = mcfg_get_device_addr(0, (uint8_t)bus, (uint8_t)slot, (uint8_t)func);
	if (addr) {
		return *(volatile uint8_t*)((uintptr_t)addr + (offset & 0xfff));
	}

	uint32_t address = (uint32_t)((((uint32_t)bus) << 16) |
					   (((uint32_t)slot) << 11) |
					   (((uint32_t)func) << 8) |
					   (offset & 0xfc) | 0x80000000);
	outl(PCI_CONFIG_ADDRESS, address);

	return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_byte(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint8_t data) {
	void* addr = mcfg_get_device_addr(0, (uint8_t)bus, (uint8_t)slot, (uint8_t)func);
	if (addr) {
		*(volatile uint8_t*)((uintptr_t)addr + (offset & 0xfff)) = data;
		return;
	}

	uint32_t address = (uint32_t)((((uint32_t)bus) << 16) |
					   (((uint32_t)slot) << 11) |
					   (((uint32_t)func) << 8) |
					   (offset & 0xfc) | 0x80000000);
	outl(PCI_CONFIG_ADDRESS, address);

	uint32_t tmp = inl(PCI_CONFIG_DATA);
	tmp &= ~(0xFF << ((offset & 3) * 8));
	tmp |= ((uint32_t)data << ((offset & 3) * 8));
	outl(PCI_CONFIG_DATA, tmp);
}

static pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
	pci_bar_t bar = {0};
	uint32_t offset = 0x10 + (bar_index * 4);

	uint32_t low = pci_read(bus, slot, func, offset);
	if (low == 0) return bar;

	bar.is_io = (low & 0x1);

	pci_write(bus, slot, func, offset, 0xFFFFFFFF);
	uint32_t readback = pci_read(bus, slot, func, offset);
	pci_write(bus, slot, func, offset, low);

	if (bar.is_io) {
		bar.base = low & ~0x3;
		bar.size = (uint64_t)(~(readback & ~0x3) + 1);
	} else {
		bar.base = low & ~0xF;

		if ((low & 0x6) == 0x4) {
			uint64_t high = pci_read(bus, slot, func, offset + 4);
			bar.base |= (high << 32);

			pci_write(bus, slot, func, offset + 4, 0xFFFFFFFF);
			uint32_t readback_high = pci_read(bus, slot, func, offset + 4);
			pci_write(bus, slot, func, offset + 4, (uint32_t)high);

			uint64_t combined_readback = (uint64_t)readback_high << 32 | (readback & ~0xF);
			bar.size = ~combined_readback + 1;
		} else {
			bar.size = (uint64_t)(~(readback & ~0xF) + 1);
		}
	}

	return bar;
}

static void pci_enable_capabilities(uint8_t bus, uint8_t slot, uint8_t func) {
	uint32_t cmd = pci_read(bus, slot, func, 0x04);

	// Bit 0: IO Space (1)
	// Bit 1: Memory Space (1)
	// Bit 2: Bus Master (1)
	// Bit 10: Interrupt Disable (0)

	// 0x07 enables bits 0, 1 and 2
	// ~(1 << 10) clears the interrupt mask bit to ensure IRQs
	cmd |= 0x07;
	cmd &= ~(1 << 10);

	pci_write(bus, slot, func, 0x04, cmd);
}

void pci_init(void) {
	int capacity = 20; // 20 as a guess for the number of devices, can grow if needed and wastes little memory if there are under 20 devices
	int count = 0;
	pci_dev_t* discovered = malloc(sizeof(pci_dev_t) * capacity);
	uint32_t claimed_groups = 0;

	// hardware sweep to cache all device info
	for (uint16_t bus = 0; bus < 256; bus++) {
		for (uint16_t slot = 0; slot < 32; slot++) {
			// check if device exists at function 0
			uint32_t id_reg = pci_read(bus, slot, 0, 0x00);
			if ((id_reg & 0xFFFF) == 0xFFFF) continue;

			uint32_t header = pci_read(bus, slot, 0, 0x0C);
			uint8_t header_type = (header >> 16) & 0xFF;
			int max_func = (header_type & 0x80) ? 8 : 1;

			for (uint8_t func = 0; func < max_func; func++) {
				id_reg = pci_read(bus, slot, func, 0x00);
				if ((id_reg & 0xFFFF) == 0xFFFF) continue;

				// expand heap buffer if hardware exceeds current capacity
				if (count >= capacity) {
					capacity *= 2;
					discovered = realloc(discovered, sizeof(pci_dev_t) * capacity);
				}

				pci_dev_t* d = &discovered[count++];
				d->bus = (uint8_t)bus;
				d->slot = (uint8_t)slot;
				d->func = func;
				d->vendor_id = (uint16_t)(id_reg & 0xFFFF);
				d->device_id = (uint16_t)(id_reg >> 16);
				d->claimed = false; // Initialize the new flag

				uint32_t class_reg = pci_read(bus, slot, func, 0x08);
				d->class_id = (class_reg >> 24) & 0xFF;
				d->subclass_id = (class_reg >> 16) & 0xFF;
				d->progif = (class_reg >> 8) & 0xFF;
				d->irq = pci_read(bus, slot, func, 0x3C) & 0xFF;

				// cache all 6 BARs
				for (int b = 0; b < 6; b++) {
					d->bars[b] = pci_get_bar(bus, slot, func, b);
					// handle 64 bit BARs
					uint32_t raw = pci_read(bus, slot, func, 0x10 + (b * 4));
					if (!(raw & 0x1) && ((raw & 0x6) == 0x4)) b++;
				}
			}
		}
	}

	// match against the driver table
	for (int i = 0; pci_discovery_table[i].name != NULL; i++) {
		pci_driver_entry_t* entry = &pci_discovery_table[i];

		if (entry->group_id != 0 && (claimed_groups & (1 << entry->group_id))) {
			continue;
		}

		for (int j = 0; j < count; j++) {
			pci_dev_t* dev = &discovered[j];

			if (dev->claimed) continue;

			bool match = true;

			// vendor id check
			if (entry->vendor_id != PCI_VENDOR_ANY && entry->vendor_id != dev->vendor_id) {
				match = false;
			}

			// device id check
			if (match && entry->device_id != PCI_DEVICE_ANY && entry->device_id != dev->device_id) {
				match = false;
			}

			// class/subclass check
			if (match && entry->class_id != PCI_ID_ANY) {
				if (dev->class_id != entry->class_id) {
					match = false;
				} else if (entry->subclass_id != PCI_ID_ANY && dev->subclass_id != entry->subclass_id) {
					match = false;
				}
			}

			// progif check
			if (match && entry->progif != PCI_ID_ANY) {
				if (dev->progif != entry->progif) {
					match = false;
				}
			}

			if (match) {
				dev->claimed = true;

				if (!pci_check_driver_capabilities(entry->required_capabilities)) {
					logbuf_printf("[ PCI  ] skipping initialization of %s  reason: Missing capabilities\n", entry->name);
					continue; // try next driver
				}

				if (entry->group_id != 0) {
					claimed_groups |= (1 << entry->group_id);
				}

				if (entry->init) {
					pci_enable_capabilities(dev->bus, dev->slot, dev->func);
					entry->init(*dev);
				} else {
					logbuf_printf("[ PCI  ] Found %s\n", entry->name);
				}

				if (entry->group_id != 0) break;
			}
		}
	}

	for (int j = 0; j < count; j++) {
		pci_dev_t* dev = &discovered[j];
		if (!dev->claimed) {
			logbuf_printf("[ PCI  ] Unhandled: %02x:%02x.%d ID %04x:%04x Class %02x/%02x\n",
						  (unsigned int)dev->bus,
						  (unsigned int)dev->slot,
						  (unsigned int)dev->func,
						  (unsigned int)dev->vendor_id,
						  (unsigned int)dev->device_id,
						  (unsigned int)dev->class_id,
						  (unsigned int)dev->subclass_id);
		}
	}

	// free the temporary cache
	free(discovered);
}

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA	0xCFC

#define PCI_VENDOR_INTEL 0x8086
#define PCI_VENDOR_REALTEK 0x10EC
#define PCI_VENDOR_VMWARE 0x15AD // ew vmware
#define PCI_VENDOR_QEMU 0x1234
#define PCI_VENDOR_ANY 0xFFFF

#define PCI_DEVICE_RTL8139 0x8139
#define PCI_DEVICE_I440FX_HB 0x1237
#define PCI_DEVICE_PIIX3_ISA 0x7000
#define PCI_DEVICE_PIIX3_IDE 0x7010
#define PCI_DEVICE_PIIX4_ACPI 0x7113
#define PCI_DEVICE_ANY 0xFFFF

// storage class 0x01
#define PCI_CLASS_STORAGE 0x01
#define PCI_SUBCLASS_IDE 0x01

// network class 0x02
#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_ETH 0x00

// multimedia class 0x04
#define PCI_CLASS_MULTIMEDIA 0x04
#define PCI_SUBCLASS_AUDIO 0x01

typedef struct {
	uintptr_t base; // physical address or IO port
	uint32_t  size;
	bool	  is_io; // true if port IO false if MMIO
} pci_bar_t;

typedef struct {
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t class_id;
	uint8_t subclass_id;
	uint8_t irq;
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	pci_bar_t bars[6];
	bool claimed;
} pci_dev_t;

typedef void (*pci_init_fn)(pci_dev_t dev);

typedef struct {
	uint16_t vendor_id;
	uint16_t device_id;

	uint8_t  class_id;
	uint8_t  subclass_id;

	// parser configuration
	uint16_t group_id; // 0 = always load if found | > 0 = only load one per group

	const char* name;
	pci_init_fn init;
} pci_driver_entry_t;

void pci_init(void);

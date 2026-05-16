#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA	0xCFC

#define PCI_VENDOR_INTEL 0x8086
#define PCI_VENDOR_REALTEK 0x10EC
#define PCI_VENDOR_VMWARE 0x15AD // ew vmware
#define PCI_VENDOR_QEMU 0x1234
#define PCI_VENDOR_AGEIA 0x1A45
#define PCI_VENDOR_SUN 0x108E
#define PCI_VENDOR_REDHAT 0x1AF4

#define PCI_DEVICE_E1000_QEMU 0x100E
#define PCI_DEVICE_RTL8139 0x8139
#define PCI_DEVICE_I440FX_HB 0x1237
#define PCI_DEVICE_PIIX3_ISA 0x7000
#define PCI_DEVICE_PIIX3_IDE 0x7010
#define PCI_DEVICE_PIIX4_ACPI 0x7113
#define PCI_DEVICE_IVSHMEM 0x1110

// storage class 0x01
#define PCI_CLASS_STORAGE 0x01
#define PCI_SUBCLASS_IDE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_SUBCLASS_NVME 0x08

// network class 0x02
#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_ETHERNET 0x00

// multimedia class 0x04
#define PCI_CLASS_MULTIMEDIA 0x04
#define PCI_SUBCLASS_AUDIO 0x01
#define PCI_SUBCLASS_HDAUDIO 0x03

// memory controller class 0x05
#define PCI_CLASS_MEMORY	   0x05
#define PCI_SUBCLASS_RAM_CTRL  0x00
#define PCI_SUBCLASS_CXL_DEVICE 0x02

// bridge class 0x06
#define PCI_CLASS_BRIDGE 0x06
#define PCI_SUBCLASS_HOST_BRIDGE 0x00
#define PCI_SUBCLASS_PCI_TO_PCI 0x04

// serial bus class 0x0C
#define PCI_CLASS_SERIAL 0x0C
#define PCI_SUBCLASS_USB 0x03

// processor class 0x0B
#define PCI_CLASS_PROCESSOR 0x0B
#define PCI_SUBCLASS_X86	0x40  // Co-processors often live here

// accelerator class 0x12
#define PCI_CLASS_ACCELERATOR 0x12
#define PCI_SUBCLASS_ACCEL_CXL 0x00

// any
#define PCI_VENDOR_ANY 0xFFFF
#define PCI_DEVICE_ANY 0xFFFF
#define PCI_ID_ANY 0xFF

// progif
#define PCI_PROGIF_ANY 0xFF
#define PCI_PROGIF_UHCI 0x00
#define PCI_PROGIF_OHCI 0x10
#define PCI_PROGIF_EHCI 0x20
#define PCI_PROGIF_XHCI 0x30
#define PCI_PROGIF_IDE_COMPAT 0x80 // legacy mode
#define PCI_PROGIF_IDE_NATIVE 0x85 // pci native mode
#define PCI_PROGIF_SATA_AHCI 0x01 // for subclass 0x06
#define PCI_PROGIF_NVME 0x02 // for subclass 0x08

typedef struct {
	uintptr_t base; // physical address or IO port
	uint64_t  size;
	bool	  is_io;
} pci_bar_t;

typedef struct {
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t class_id;
	uint8_t subclass_id;
	uint8_t progif;
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

	uint8_t class_id;
	uint8_t subclass_id;
	uint8_t progif;

	// parser configuration
	uint16_t group_id; // 0 = always load if found | > 0 = only load one per group

	const char* name;
	pci_init_fn init;
} pci_driver_entry_t;

// 32 bit access
uint32_t pci_read(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);
void pci_write(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t data);

// 8 bit access
uint8_t pci_read_byte(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);
void pci_write_byte(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint8_t data);

void pci_init(void);

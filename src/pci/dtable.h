#pragma once

#include "pci/pci.h"
#include "pci/drivers/ide.h"
#include "pci/drivers/ac97.h"

pci_driver_entry_t pci_discovery_table[] = {
	// Host Bridge
	{
		.name = "Host Bridge",
		.group_id = 0,
		.vendor_id = PCI_VENDOR_ANY,
		.device_id = PCI_DEVICE_ANY,
		.class_id = PCI_CLASS_BRIDGE,
		.subclass_id = PCI_SUBCLASS_HOST_BRIDGE,
		.progif = PCI_PROGIF_ANY,

		.init = NULL // no driver for this
	},
	{
		.name = "AC'97 Audio Controller",
		.group_id = 1, // this will ensure that if we have more audio drivers, the first one in the table will be initalized and others will not
		.vendor_id = PCI_VENDOR_ANY,
		.device_id = PCI_DEVICE_ANY,
		.class_id = PCI_CLASS_MULTIMEDIA,
		.subclass_id = PCI_SUBCLASS_AUDIO,
		.progif = PCI_PROGIF_ANY,

		#ifdef KERNEL_MOD_AC97_ENABLED
			.init = ac97_init
		#else
			.init = NULL
		#endif
	},

	// IDE with any vendor (disk controllers should be last in the table, as they initalize filesystems and it can make logs look messy)
	{
		.name = "Generic IDE Controller",
		.group_id = 0,
		.vendor_id = PCI_VENDOR_ANY,
		.device_id = PCI_DEVICE_ANY,
		.class_id = PCI_CLASS_STORAGE,
		.subclass_id = PCI_SUBCLASS_IDE,
		.progif = PCI_PROGIF_ANY,

		#ifdef KERNEL_MOD_IDE_ENABLED
			.init = ide_init
		#else
			.init = NULL
		#endif
	},
	{
		.name = "Generic SATA Controller",
		.group_id = 0,
		.vendor_id = PCI_VENDOR_ANY,
		.device_id = PCI_DEVICE_ANY,
		.class_id = PCI_CLASS_STORAGE,
		.subclass_id = PCI_SUBCLASS_SATA,
		.progif = PCI_PROGIF_ANY,

		.init = NULL
	},
	{ .vendor_id = 0, .device_id = 0, .name = NULL, .init = NULL }
};

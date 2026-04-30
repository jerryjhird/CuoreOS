#include "pci/pci.h"

#ifdef KERNEL_MOD_IDE_ENABLED
#include "pci/drivers/ide.h"
#endif

#ifdef KERNEL_MOD_RTL8139_ENABLED
#include "pci/drivers/rtl8139.h"
#endif

#ifdef KERNEL_MOD_AC97_ENABLED
#include "pci/drivers/ac97.h"
#endif

#ifdef KERNEL_MOD_AHCI_ENABLED
#include "pci/drivers/ahci.h"
#endif

#ifdef KERNEL_MOD_IVSHMEM_ENABLED
#include "pci/drivers/ivshem.h"
#endif

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
		.name = "ivshmem",
		.group_id = 0,
		.vendor_id = PCI_VENDOR_REDHAT,
		.device_id = PCI_DEVICE_IVSHMEM,
		.class_id = PCI_CLASS_MEMORY,
		.subclass_id = PCI_SUBCLASS_RAM_CTRL,
		.progif = PCI_PROGIF_ANY,
		#ifdef KERNEL_MOD_IVSHMEM_ENABLED
			.init = ivshmem_init
		#else
			.init = NULL
		#endif
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
	{
		.name = "RTL8139 Ethernet Controller",
		.group_id = 2,
		.vendor_id = PCI_VENDOR_REALTEK,
		.device_id = PCI_DEVICE_RTL8139,
		.class_id = PCI_CLASS_NETWORK,
		.subclass_id = PCI_SUBCLASS_ETHERNET,
		.progif = PCI_PROGIF_ANY,

		#ifdef KERNEL_MOD_RTL8139_ENABLED
			.init = rtl8139_init
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

		#ifdef KERNEL_MOD_AHCI_ENABLED
			.init = ahci_init
		#else
			.init = NULL
		#endif
	},
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
	{ .vendor_id = 0, .device_id = 0, .name = NULL, .init = NULL }
};

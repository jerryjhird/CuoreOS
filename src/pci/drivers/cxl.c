typedef int dummy0;

// driver for CXL
#include "Config.h"

#ifdef KERNEL_MOD_CXL_ENABLED
#include "cxl.h"
#include "pci/pci.h"
#include "mem/paging.h"
#include "mem/vmm.h"
#include <stddef.h>
#include "logbuf.h"
#include "mem/mem.h"
#include "device/types.h"
#include "mem/heap.h"
#include "panic.h"
#include "firmware/acpi/cedt.h"
#include "device/devreg.h"

static extmem_type_t determine_cxl_type(pci_dev_t dev) {
	if (dev.class_id == 0x05 && dev.subclass_id == 0x02) {
		return CXL_TYPE3_VOLATILE;
	} else if (dev.class_id == 0x12) {
		return CXL_TYPE2_ACCEL_MEM;
	} else {
		return CXL_TYPE3_VOLATILE;
	}
}

pci_driver_status cxl_init(pci_dev_t dev) {
	cedt_ret_t window = {0};

	if (!cedt_get_info(&window) || !window.found) { return CORRUPTED_FIRMWARE_TABLE; }

	size_t pages = (window.size + PAGE_SIZE - 1) / PAGE_SIZE;
	uintptr_t virt = vmm_alloc_pages(pages);
	if (!virt) { panic("CXL", "vmm failed to allocate virtual region"); }

	uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);

	for (size_t i = 0; i < pages; i++) {
		vmm_map_page_noflush(pml4,
			virt + i * PAGE_SIZE,
			window.phys_base + i * PAGE_SIZE,
			PTE_PRESENT | PTE_WRITABLE);
	}

	vmm_flush_tlb_all();

	kernel_extmem_dev_t *ext_dev = zalloc(sizeof(kernel_extmem_dev_t));
	pci_dev_t *dev_copy = zalloc(sizeof(pci_dev_t));

	__builtin_memcpy(dev_copy, &dev, sizeof(pci_dev_t));

	ext_dev->type = determine_cxl_type(dev);
	ext_dev->virt_addr = virt;
	ext_dev->phys_addr = window.phys_base;
	ext_dev->size = window.size;
	ext_dev->private_data = dev_copy;

	const char *type_str =
		ext_dev->type == CXL_TYPE2_ACCEL_MEM  ? "Type 2 Accelerator" :
		ext_dev->type == CXL_TYPE3_PERSISTENT  ? "Type 3 Persistent"  :
											 "Type 3 Volatile";

	device_register(EXTMEM_DEV, ext_dev);

	logbuf_ok("[ CXL  ] Initialized CXL Device\n"
			  "		Device type: %s\n"
			  "		Host Bridge UID: %d\n"
			  "		Physical base: %#lx\n"
			  "		Virtual base:  %#lx\n"
			  "		Size: %zu MB\n",
			  type_str,
			  window.host_bridge_uid,
			  window.phys_base,
			  virt,
			  (size_t)BYTES_TO_MB(window.size));
	return DRIVER_OK;
}
#endif

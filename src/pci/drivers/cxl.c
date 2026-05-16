typedef int dummy0;

#include "Config.h"

#ifdef KERNEL_MOD_CXL_ENABLED
#include "cxl.h"
#include "pci/pci.h"
#include "mem/paging.h"
#include <stddef.h>
#include "logbuf.h"
#include "mem/mem.h"
#include "devices.h"
#include "mem/heap.h"
#include "panic.h"

#include "acpi/cedt.h"

#define EXTMEM_VIRT_ALLOC_BASE 0xffffc00000000000

static uint64_t cxl_virt_alloc_ptr = EXTMEM_VIRT_ALLOC_BASE;

static extmem_type_t determine_cxl_type(pci_dev_t dev) {
	if (dev.class_id == 0x05 && dev.subclass_id == 0x02) {
		return CXL_TYPE3_VOLATILE;
	} else if (dev.class_id == 0x12) {
		return CXL_TYPE2_ACCEL_MEM;
	} else {
		return CXL_TYPE3_VOLATILE;
	}
}

void cxl_init(pci_dev_t dev) {
	acpi_sdt_header_t* cedt = (acpi_sdt_header_t*)acpi_find_sdt("CEDT");
	if (!cedt) {
		panic("CXL", "CEDT table missing");
	}

	cedt_ret_t window = {0};
	parse_cedt(cedt, &window);

	if (!window.found || window.size == 0 || window.phys_base == 0) {
		panic("CXL", "failed to resolve valid CXL fixed memory window (CFMWS)");
	}

	uintptr_t pml4_phys = vmm_get_pml4();
	uint64_t* pml4_virt = (uint64_t*)(pml4_phys + hhdm_offset);

	uint64_t virt_addr = cxl_virt_alloc_ptr;
	cxl_virt_alloc_ptr += window.size;

	for (size_t i = 0; i < window.size; i += 4096) {
		vmm_map_page_noflush(pml4_virt,
							 virt_addr + i,
							 window.phys_base + i,
							 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER);
	}
	vmm_flush_tlb_all();

	kernel_extmem_dev_t* ext_dev = zalloc(sizeof(kernel_extmem_dev_t));

	ext_dev->type = determine_cxl_type(dev);
	ext_dev->virt_addr = virt_addr;
	ext_dev->phys_addr = window.phys_base;
	ext_dev->size = window.size;

	ext_dev->private_data = zalloc(sizeof(pci_dev_t));
	__builtin_memcpy(ext_dev->private_data, &dev, sizeof(pci_dev_t));

	extmem_devices[extmem_devices_c++] = ext_dev;

	const char* type_str = (ext_dev->type == CXL_TYPE2_ACCEL_MEM) ? "Type 2 Accelerator" :
						   (ext_dev->type == CXL_TYPE3_PERSISTENT) ? "Type 3 Persistent" :
																	 "Type 3 Volatile";

	logbuf_printf("[ CXL  ] Compute Express Link initialized successfully\n"
							"	  Device type:	 %s\n"
							"	  Host Bridge UID: %d\n"
							"	  Physical base:   %#lx\n"
							"	  Virtual base:	%#lx\n"
							"	  size:			%zu MB\n",
				  type_str,
				  window.host_bridge_uid,
				  window.phys_base,
				  virt_addr,
				  window.size / (1024 * 1024));
}
#endif

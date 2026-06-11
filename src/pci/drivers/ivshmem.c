typedef int dummy0;

// driver for ivshmem
#include "Config.h"

#ifdef KERNEL_MOD_IVSHMEM_ENABLED
#include "ivshmem.h"
#include "pci/pci.h"
#include "mem/paging.h"
#include "mem/vmm.h"
#include <stddef.h>
#include "logbuf.h"
#include "mem/mem.h"
#include "device/types.h"
#include "mem/heap.h"
#include "device/devreg.h"

pci_driver_status ivshmem_init(pci_dev_t dev) {
	if (!dev.bars[2].base || !dev.bars[2].size) {
		return INVALID_BAR;
	}

	uintptr_t phys = dev.bars[2].base;
	size_t size = dev.bars[2].size;
	size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	uintptr_t virt = vmm_alloc_pages(pages);

	uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);
	for (size_t i = 0; i < pages; i++) {
		vmm_map_page_noflush(pml4,
			virt + i * PAGE_SIZE,
			phys + i * PAGE_SIZE,
			PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
	}

	vmm_flush_tlb_all();

	kernel_extmem_dev_t *ext = zalloc(sizeof(kernel_extmem_dev_t));

	ext->type = IVSHMEM_NON_PERSISTENT;
	ext->virt_addr = virt;
	ext->phys_addr = phys;
	ext->size = size;
	ext->private_data = NULL;

	device_register(EXTMEM_DEV, ext);

	logbuf_ok("[ SHM  ] Initialized IVSHMEM\n"
						"	  Physical base: %#lx\n"
						"	  Virtual base:	%#lx\n"
						"	  Size: %zu MB\n",
			  			phys,
			  			virt,
			  			size / (1024 * 1024));
	return DRIVER_OK;
}
#endif

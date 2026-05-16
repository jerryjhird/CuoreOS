typedef int dummy0;

// driver for ivshmem
#include "Config.h"

#ifdef KERNEL_MOD_IVSHMEM_ENABLED
#include "ivshmem.h"
#include "pci/pci.h"
#include "mem/paging.h"
#include <stddef.h>
#include "logbuf.h"
#include "mem/mem.h"
#include "devices.h"
#include "mem/heap.h"

void ivshmem_init(pci_dev_t dev) {
	uint64_t phys_addr = dev.bars[2].base;
	size_t shm_size = dev.bars[2].size;

	uintptr_t pml4_phys = vmm_get_pml4();
	uint64_t* pml4_virt = (uint64_t*)(pml4_phys + hhdm_offset);

	for (size_t i = 0; i < shm_size; i += 4096) {
		vmm_map_page_noflush(pml4_virt,
					 phys_addr + i + hhdm_offset,
					 phys_addr + i,
					 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE);
	}

	vmm_flush_tlb_all();

	uint64_t virt_addr = phys_addr + hhdm_offset;

	kernel_extmem_dev_t* ext_dev = zalloc(sizeof(kernel_extmem_dev_t));

	ext_dev->type = IVSHMEM_NON_PERSISTENT; // assume non persistent
	ext_dev->virt_addr = virt_addr;
	ext_dev->phys_addr = phys_addr;
	ext_dev->size = shm_size;
	ext_dev->private_data = NULL;

	if (extmem_devices_c < MAX_EXTMEM_DEVICES) {
		extmem_devices[extmem_devices_c++] = ext_dev;
	}

	logbuf_write("[ SHM  ] IVSHMEM initialized\n");
}
#endif

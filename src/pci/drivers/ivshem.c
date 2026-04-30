#include "ivshem.h"
#include "pci/pci.h"
#include "mem/paging.h"
#include <stddef.h>
#include "logbuf.h"
#include "mem/mem.h"
#include "devices.h"
#include "mem/heap.h"

void ivshmem_init(pci_dev_t dev) {
	uintptr_t phys_addr = dev.bars[2].base;
	size_t shm_size = dev.bars[2].size;

	uintptr_t pml4_phys = vmm_get_pml4();
	uint64_t* pml4_virt = (uint64_t*)(pml4_phys + hhdm_offset);

	for (size_t i = 0; i < shm_size; i += 4096) {
		vmm_map_page(pml4_virt,
					 phys_addr + i + hhdm_offset,
					 phys_addr + i,
					 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE);
	}

	void* virt_addr = (void*)(phys_addr + hhdm_offset);

	kernel_extmem_dev_t* ext_dev = malloc(sizeof(kernel_extmem_dev_t));

	// allocate and fill device struct
	memset(ext_dev->model, 0, 32);
	memcpy(ext_dev->model, "QEMU ivshmem", 13);

	ext_dev->type = HOST_TO_GUEST;
	ext_dev->virt_addr = virt_addr;
	ext_dev->phys_addr = phys_addr;
	ext_dev->size = shm_size;
	ext_dev->private_data = NULL;

	if (extmem_devices_c < MAX_EXTMEM_DEVICES) {
		extmem_devices[extmem_devices_c++] = ext_dev;
	}

	logbuf_write("[ SHM  ] ivshmem initialized\n");

	const char* sig = "ALIVE";
	for(int i = 0; i < 5; i++) {
		((char*)virt_addr)[i] = sig[i];
	}
}

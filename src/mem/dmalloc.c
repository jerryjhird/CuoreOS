#include "dmalloc.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "mem/heap.h"

static DMARegistry *registry_head = NULL;

static void register_dma(uintptr_t virt, size_t pages) {
	DMARegistry *entry = malloc(sizeof(DMARegistry));
	entry->virt = virt;
	entry->pages = pages;
	entry->next = registry_head;
	registry_head = entry;
}

dmalloc_ret_t dmalloc(size_t size) {
	size_t pages = (size + 4095) / 4096;
	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) return (dmalloc_ret_t){0, 0};

	uintptr_t virt = phys + hhdm_offset;
	register_dma(virt, pages);

	return (dmalloc_ret_t){ .virt = virt, .phys = phys };
}

dmalloc_ret_t dmalloc32(size_t size) {
	size_t pages = (size + 4095) / 4096;
	uintptr_t phys = pma_alloc_pages_low(pages);
	if (!phys) return (dmalloc_ret_t){0, 0};

	uintptr_t virt = phys + hhdm_offset;
	register_dma(virt, pages);

	return (dmalloc_ret_t){ .virt = virt, .phys = phys };
}

void dmfree(uint64_t virt_addr) {
	if (!virt_addr) return;

	DMARegistry *curr = registry_head;
	DMARegistry *prev = NULL;

	while (curr) {
		if (curr->virt == (uintptr_t)virt_addr) {
			pma_free_pages(curr->virt - hhdm_offset, curr->pages);

			if (prev) prev->next = curr->next;
			else registry_head = curr->next;

			free(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

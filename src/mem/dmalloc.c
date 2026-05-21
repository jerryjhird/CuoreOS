#include "dmalloc.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "abs.h"

static DMABlock *dma_head = NULL;

static dmalloc_ret_t _dmalloc_internal(uint64_t size, bool low_mem) {
	size = ALIGN(16, size);
	dmalloc_ret_t fail = {0, 0};

	for (DMABlock *curr = dma_head; curr != NULL; curr = curr->next) {
		if (curr->is_free && curr->size >= size) {
			if (curr->size >= size + sizeof(DMABlock) + 16) {
				DMABlock *split = (DMABlock*)((uint64_t)curr + sizeof(DMABlock) + size);
				split->magic = DMA_MAGIC;
				split->size = curr->size - size - sizeof(DMABlock);
				split->phys_addr = curr->phys_addr + sizeof(DMABlock) + size;
				split->is_free = true;
				split->next = curr->next;
				split->prev = curr;
				if (curr->next) curr->next->prev = split;
				curr->next = split;
				curr->size = size;
			}

			curr->is_free = false;
			return (dmalloc_ret_t){
				.virt = (uint64_t)curr + sizeof(DMABlock),
				.phys = curr->phys_addr + sizeof(DMABlock)
			};
		}
	}

	// grow
	uint64_t pages = (size + sizeof(DMABlock) + 4095) / 4096;
	uint64_t new_phys = low_mem ? pma_alloc_pages_low(pages) : pma_alloc_pages(pages);
	if (!new_phys) return fail;

	DMABlock *new_block = (DMABlock*)(new_phys + hhdm_offset);
	new_block->magic = DMA_MAGIC;
	new_block->size = (pages * 4096) - sizeof(DMABlock);
	new_block->phys_addr = new_phys;
	new_block->is_free = true;
	new_block->next = NULL;

	if (!dma_head) {
		new_block->prev = NULL;
		dma_head = new_block;
	} else {
		DMABlock *last = dma_head;
		while (last->next) last = last->next;
		last->next = new_block;
		new_block->prev = last;
	}

	return _dmalloc_internal(size, low_mem);
}

dmalloc_ret_t dmalloc(uint64_t size) { return _dmalloc_internal(size, false); }
dmalloc_ret_t dmalloc32(uint64_t size) { return _dmalloc_internal(size, true); }

void dmfree(uint64_t virt_addr) {
	if (!virt_addr) return;
	DMABlock *curr = (DMABlock*)(virt_addr - sizeof(DMABlock));
	if (curr->magic != DMA_MAGIC) return;

	curr->is_free = true;

	if (curr->next && curr->next->is_free && ((uint64_t)curr + sizeof(DMABlock) + curr->size == (uint64_t)curr->next)) {
		curr->size += sizeof(DMABlock) + curr->next->size;
		curr->next = curr->next->next;
		if (curr->next) curr->next->prev = curr;
	}

	if (curr->prev && curr->prev->is_free && ((uint64_t)curr->prev + sizeof(DMABlock) + curr->prev->size == (uint64_t)curr)) {
		DMABlock *p = curr->prev;
		p->size += sizeof(DMABlock) + curr->size;
		p->next = curr->next;
		if (curr->next) curr->next->prev = p;

		curr = p;
	}
}

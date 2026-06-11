#include "heap.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "mem.h"
#include "pma.h"
#include "stdio.h"
#include "mem/paging.h"
#include "abs.h"
#include "panic.h"
#include "vmm.h"
#include "logbuf.h"

#define POOL_COUNT 3
static size_t pool_sizes[POOL_COUNT] = { 32, 64, 128 };

typedef struct PoolNode {
	struct PoolNode *next;
} PoolNode;

typedef struct {
	size_t block_size;
	PoolNode *free_list;
} FixedPool;

// flags for BlockHeader
#define BH_FREE		   (1u << 0)
#define BH_POOL_BLOCK	 (1u << 1)
#define BH_BOUNDARY_START (1u << 2)

typedef struct BlockHeader {
	uint32_t magic;
	uint32_t flags;
	size_t size;
	size_t bin_size;
	uint32_t used_slots;
	uint32_t _pad;
	struct BlockHeader *next;
	struct BlockHeader *prev;
} BlockHeader;

#define BH_IS_FREE(b) ((b)->flags & BH_FREE)
#define BH_IS_POOL(b) ((b)->flags & BH_POOL_BLOCK)
#define BH_IS_BOUNDARY(b) ((b)->flags & BH_BOUNDARY_START)
#define BH_SET_FREE(b) ((b)->flags |= BH_FREE)
#define BH_CLR_FREE(b) ((b)->flags &= ~BH_FREE)

static BlockHeader *head = NULL;
static BlockHeader *tail = NULL;
static BlockHeader *last_fit = NULL;
static FixedPool pools[POOL_COUNT];

#define MAX_POOL_PAGES 64
static uintptr_t pool_page_registry[MAX_POOL_PAGES];
static size_t registered_pool_pages = 0;

static bool is_pool_page(uintptr_t page_base) {
	for (size_t i = 0; i < registered_pool_pages; i++) {
		if (pool_page_registry[i] == page_base) { return true; }
	}
	return false;
}

static uint8_t get_ptr_type(void *ptr) {
	uintptr_t page_base = (uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1);
	if (is_pool_page(page_base)) { return PTE_STATE_HEAP_POOL; }
	BlockHeader *block = ((BlockHeader *)ptr) - 1;
	if (block->magic == HEAP_MAGIC) { return PTE_STATE_HEAP_BLOCK; }
	return PTE_STATE_NOINFO;
}

static void list_append(BlockHeader *bh) {
	bh->next = NULL;
	bh->prev = tail;
	if (tail) { tail->next = bh; }
	tail = bh;
	if (!head) { head = bh; }
}

static void list_unlink(BlockHeader *bh) {
	if (bh->prev) { bh->prev->next = bh->next; }
	else { head = bh->next; }
	if (bh->next) { bh->next->prev = bh->prev; }
	else { tail = bh->prev; }
	if (last_fit == bh) { last_fit = NULL; }
}

static bool heap_grow(size_t size_needed) {
	size_t pages = (size_needed + sizeof(BlockHeader) + PAGE_SIZE - 1) / PAGE_SIZE;
	if (pages < PAGES_PER_GROWTH) { pages = PAGES_PER_GROWTH; }

	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) { return false; }

	uintptr_t virt = vmm_alloc_pages(pages);
	if (!virt) {
		pma_free_pages(phys, pages);
		return false;
	}

	uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);
	for (size_t i = 0; i < pages; i++) {
		vmm_map_page_noflush(pml4, virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_BLOCK);
	}
	vmm_flush_tlb_all();

	BlockHeader *bh = (BlockHeader *)virt;
	*bh = (BlockHeader){0};
	bh->magic = HEAP_MAGIC;
	bh->size = pages * PAGE_SIZE - sizeof(BlockHeader);
	bh->flags = BH_FREE | BH_BOUNDARY_START;
	list_append(bh);
	return true;
}

static void heap_init_pools(void) {
	uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);

	for (int i = 0; i < POOL_COUNT; i++) {
		pools[i].block_size = pool_sizes[i];
		pools[i].free_list = NULL;

		uintptr_t phys = pma_alloc_pages(1);
		if (!phys) { panic("HEAP", "pma failed for pool page"); }

		uintptr_t virt = vmm_alloc_pages(1);
		if (!virt) { panic("HEAP", "vmm failed for pool page"); }

		vmm_map_page(pml4, virt, phys, KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_POOL);

		if (registered_pool_pages >= MAX_POOL_PAGES) {
			panic("HEAP", "pool page registry exhausted");
		}

		pool_page_registry[registered_pool_pages++] = virt;

		BlockHeader *bh = (BlockHeader *)virt;
		*bh = (BlockHeader){0};
		bh->magic = HEAP_MAGIC;
		bh->flags = BH_POOL_BLOCK;
		bh->bin_size = pool_sizes[i];
		bh->size = PAGE_SIZE - sizeof(BlockHeader);
		bh->used_slots = 0;
		list_append(bh);

		_Static_assert(sizeof(BlockHeader) % 8 == 0, "BlockHeader must be 8-byte aligned for pool node casting");

		uint8_t *data = (uint8_t *)__builtin_assume_aligned((uint8_t *)(bh + 1), 8);
		size_t slots = bh->size / pool_sizes[i];

		for (size_t j = 0; j < slots; j++) {
			PoolNode *node = (PoolNode *)(uintptr_t)(data + j * pool_sizes[i]);
			node->next = pools[i].free_list;
			pools[i].free_list = node;
		}
	}
}

void heap_init(size_t size) {
	size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	uintptr_t virt = vmm_alloc_pages(pages);
	if (!virt) { panic("HEAP", "vmm failed for heap init"); }

	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) { panic("HEAP", "pma failed for heap init"); }

	uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);

	for (size_t i = 0; i < pages; i++) {
		vmm_map_page_noflush(pml4, virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_BLOCK);
	}

	vmm_flush_tlb_all();

	head = (BlockHeader *)virt;
	*head = (BlockHeader){0};
	head->magic = HEAP_MAGIC;
	head->size = pages * PAGE_SIZE - sizeof(BlockHeader);
	head->flags = BH_FREE | BH_BOUNDARY_START;
	head->next = NULL;
	head->prev = NULL;
	tail = head;

	logbuf_ok("[ HEAP ] Initialized HEAP with %zu pages (%zu MB) starting at %p\n", pages, (size_t)BYTES_TO_MB(pages * PAGE_SIZE), (void *)virt);
	heap_init_pools();
}

void *malloc(size_t size) {
	if (UNLIKELY(size == 0)) { return NULL; }

	for (int i = 0; i < POOL_COUNT; i++) {
		if (size <= pools[i].block_size && pools[i].free_list) {
			void *ptr = pools[i].free_list;
			pools[i].free_list = ((PoolNode *)ptr)->next;
			BlockHeader *bh = (BlockHeader *)((uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1));
			bh->used_slots++;
			return ptr;
		}
	}

	size = ALIGN(HEAP_ALIGNMENT, size);

	retry:;
	BlockHeader *start = last_fit ? last_fit : head;
	BlockHeader *curr = start;
	bool wrapped = false;

	while (curr) {
		if (UNLIKELY(curr->magic != HEAP_MAGIC)) { panic("MEMORY CORRUPTION", "heap magic invalid"); }

		if (BH_IS_FREE(curr) && !BH_IS_POOL(curr) && curr->size >= size) {
			if (curr->size >= size + sizeof(BlockHeader) + MIN_SPLIT_SIZE) {
				BlockHeader *split = (BlockHeader *)((uintptr_t)(curr + 1) + size);
				*split = (BlockHeader){0};
				split->magic = HEAP_MAGIC;
				split->size = curr->size - size - sizeof(BlockHeader);
				split->flags = BH_FREE;
				split->next = curr->next;
				split->prev = curr;
				if (curr->next) { curr->next->prev = split; }
				else { tail = split; }
				curr->next = split;
				curr->size = size;
			}
			BH_CLR_FREE(curr);
			last_fit = curr;
			return (void *)(curr + 1);
		}

		curr = curr->next;
		if (!curr && !wrapped) {
			// only wrap if didnt start at head
			if (start != head) {
				curr = head;
				wrapped = true;
			} else {
				break;
			}
		}
		if (curr == start) { break; }
	}

	if (heap_grow(size)) { goto retry; }
	return NULL;
}

static void _free(void *ptr, uint8_t type) {
	if (UNLIKELY(!ptr)) { return; }

	if (type == PTE_STATE_HEAP_POOL) {
		BlockHeader *bh = (BlockHeader *)((uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1));
		size_t bin = bh->bin_size;
		int bin_idx = -1;
		for (int i = 0; i < POOL_COUNT; i++) {
			if (pools[i].block_size == bin) { bin_idx = i; break; }
		}
		if (bin_idx == -1) { panic("MEMORY CORRUPTION", "pool bin mismatch"); }

		PoolNode *node = (PoolNode *)ptr;
		node->next = pools[bin_idx].free_list;
		pools[bin_idx].free_list = node;
		if (bh->used_slots > 0) { bh->used_slots--; }

		if (bh->used_slots == 0) {
			uintptr_t page_base = (uintptr_t)bh;
			PoolNode *prev_node = NULL;
			PoolNode *curr_node = pools[bin_idx].free_list;
			while (curr_node) {
				PoolNode *next = curr_node->next;
				if (((uintptr_t)curr_node & ~(uintptr_t)(PAGE_SIZE - 1)) == page_base) {
					if (prev_node) { prev_node->next = next; }
					else { pools[bin_idx].free_list = next; }
				} else {
					prev_node = curr_node;
				}
				curr_node = next;
			}

			// remove from registry
			for (size_t i = 0; i < registered_pool_pages; i++) {
				if (pool_page_registry[i] == page_base) {
					pool_page_registry[i] = pool_page_registry[--registered_pool_pages];
					break;
				}
			}

			// get phys before unmapping
			uint64_t *pml4 = (uint64_t *)(vmm_get_pml4() + hhdm_offset);
			uint64_t *pte = vmm_get_pte(pml4, page_base, 0);
			uintptr_t phys = pte ? (*pte & ~(uintptr_t)0xfff) : 0;

			list_unlink(bh);
			vmm_free_pages(page_base, 1);
			if (phys) { pma_free_pages(phys, 1); }
		}
		return;
	}

	if (type == PTE_STATE_HEAP_BLOCK) {
		BlockHeader *block = ((BlockHeader *)ptr) - 1;
		if (block->magic != HEAP_MAGIC) { panic("MEMORY CORRUPTION", "heap magic mismatch on free"); }

		BH_SET_FREE(block);

		// Forward Coalesce:
		if (block->next && BH_IS_FREE(block->next) &&
			!BH_IS_POOL(block->next) && !BH_IS_BOUNDARY(block->next)) {
			BlockHeader *fwd = block->next;
			block->size += sizeof(BlockHeader) + fwd->size;
			block->next = fwd->next;
			if (fwd->next) { fwd->next->prev = block; }
			else { tail = block; }
			if (last_fit == fwd) { last_fit = block; }
		}

		// Backward Coalesce:
		if (block->prev && BH_IS_FREE(block->prev) &&
			!BH_IS_POOL(block->prev) && !BH_IS_BOUNDARY(block)) {
			BlockHeader *bwd = block->prev;
			bwd->size += sizeof(BlockHeader) + block->size;
			bwd->next = block->next;
			if (block->next) { block->next->prev = bwd; }
			else { tail = bwd; }
			if (last_fit == block) { last_fit = bwd; }
		}
		return;
	}

	panic("MEMORY CORRUPTION", "free called with unknown block type");
}

void *realloc(void *ptr, size_t new_size) {
	if (UNLIKELY(!ptr)) { return malloc(new_size); }
	if (UNLIKELY(new_size == 0)) { _free(ptr, get_ptr_type(ptr)); return NULL; }

	new_size = ALIGN(HEAP_ALIGNMENT, new_size);
	uint8_t type = get_ptr_type(ptr);

	if (type == PTE_STATE_HEAP_POOL) {
		BlockHeader *bh = (BlockHeader *)((uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1));
		size_t old_size = bh->bin_size;
		if (new_size <= old_size) { return ptr; }
		void *new_p = malloc(new_size);

		if (new_p) {
			memcpy(new_p, ptr, old_size);
			_free(ptr, PTE_STATE_HEAP_POOL);
		}

		return new_p;
	}

	if (type == PTE_STATE_HEAP_BLOCK) {
		BlockHeader *curr = ((BlockHeader *)ptr) - 1;
		if (curr->magic != HEAP_MAGIC) { panic("MEMORY CORRUPTION", "realloc on bad magic"); }

		size_t old_size = curr->size;
		if (old_size >= new_size) { return ptr; }

		// try in place expansion into next block
		if (curr->next && BH_IS_FREE(curr->next) && !BH_IS_POOL(curr->next) &&
			!BH_IS_BOUNDARY(curr->next)) {
			size_t combined = curr->size + sizeof(BlockHeader) + curr->next->size;
			if (combined >= new_size) {
				if (combined >= new_size + sizeof(BlockHeader) + MIN_SPLIT_SIZE) {
					BlockHeader *old_next = curr->next;
					curr->size = new_size;
					BlockHeader *split = (BlockHeader *)((uintptr_t)(curr + 1) + new_size);
					*split = (BlockHeader){0};
					split->magic = HEAP_MAGIC;
					split->size = combined - new_size - sizeof(BlockHeader);
					split->flags = BH_FREE;
					split->next = old_next->next;
					split->prev = curr;
					if (split->next) { split->next->prev = split; }
					else { tail = split; }
					curr->next = split;
					if (last_fit == old_next) { last_fit = split; }
				} else {
					if (last_fit == curr->next) { last_fit = curr; }
					curr->size = combined;
					curr->next = curr->next->next;
					if (curr->next) { curr->next->prev = curr; }
					else { tail = curr; }
				}
				return ptr;
			}
		}

		void *new_ptr = malloc(new_size);
		if (new_ptr) {
			memcpy(new_ptr, ptr, old_size);
			_free(ptr, PTE_STATE_HEAP_BLOCK);
		}

		return new_ptr;
	}

	panic("REALLOC ERROR", "realloc on unmanaged pointer");
	return NULL;
}

void free(void *ptr) {
	if (UNLIKELY(!ptr)) { return; }

	uint8_t type = get_ptr_type(ptr);
	if (UNLIKELY(type == PTE_STATE_NOINFO)) { panic("MEMORY CORRUPTION", "free on unmanaged pointer"); }

	_free(ptr, type);
}

void *zalloc(size_t size) {
	void *p = malloc(size);
	if (p) { memset(p, 0, size); }

	return p;
}

void zfree(void *ptr) {
	if (UNLIKELY(!ptr)) { return; }

	uint8_t type = get_ptr_type(ptr);
	if (UNLIKELY(type == PTE_STATE_NOINFO)) { panic("MEMORY CORRUPTION", "zfree on unmanaged pointer"); }

	size_t size = 0;

	if (type == PTE_STATE_HEAP_POOL) {
		size = ((BlockHeader *)((uintptr_t)ptr & ~(uintptr_t)(PAGE_SIZE - 1)))->bin_size;
	} else if (type == PTE_STATE_HEAP_BLOCK) {
		size = (((BlockHeader *)ptr) - 1)->size;
	}

	if (size > 0) { memset(ptr, 0, size); }
	_free(ptr, type);
}

void sfree(void *ptr, size_t size) {
	UNUSED(size);
	free(ptr);
}

void szfree(void *ptr, size_t size) {
	UNUSED(size);
	zfree(ptr);
}

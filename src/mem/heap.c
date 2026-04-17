#include "heap.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "mem.h"
#include "pma.h"
#include "kstate.h"
#include "stdio.h"
#include "mem/paging.h"

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEAP_MAGIC 0xCAFEBABE
#define MIN_SPLIT_SIZE 16
#define PAGES_PER_GROWTH 4

#define KERNEL_HEAP_START 0xFFFFFFFF90000000 // where the kernel heap starts in virtual memory
#define KERNEL_HEAP_FLAGS (PTE_PRESENT | PTE_WRITABLE | PTE_NX)

// Pool configuration for small objects
#define POOL_COUNT 3
static size_t pool_sizes[POOL_COUNT] = { 32, 64, 128 };
static uintptr_t heap_current_top = KERNEL_HEAP_START;

typedef struct PoolNode {
	struct PoolNode* next;
} PoolNode;

typedef struct {
	size_t block_size;
	PoolNode* free_list;
} FixedPool;

typedef struct BlockHeader {
	uint32_t magic;
	uint32_t pad0;
	size_t size;
	uint8_t is_free;
	uint8_t is_pool_block;
	uint16_t pad1;
	uint32_t used_slots;

	size_t bin_size;

	struct BlockHeader *next;
	struct BlockHeader *prev;

	uint64_t reserved[2];
} BlockHeader;

_Static_assert(sizeof(BlockHeader) == 64, "BlockHeader must be exactly 64 bytes for 16-byte alignment");

static BlockHeader *head = NULL;
static BlockHeader *last_fit = NULL;
static FixedPool pools[POOL_COUNT];

#define MAX_POOL_PAGES 64
static uintptr_t pool_page_registry[MAX_POOL_PAGES];
static int registered_pool_pages = 0;

void panic(const char* header_msg, const char* msg);

static inline uint8_t get_ptr_type(void* ptr, uint64_t** out_pte) {
	uint64_t* pml4_v = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	uint64_t* pte = vmm_get_pte(pml4_v, (uintptr_t)ptr, 0);
	if (out_pte) *out_pte = pte;
	if (!pte || !(*pte & 1)) return PTE_STATE_NOINFO;
	return (uint8_t)((*pte >> 9) & 0x07);
}

static bool heap_grow(size_t size_needed) {
	size_t pages = (size_needed + sizeof(BlockHeader) + 4095) / 4096;
	if (pages < PAGES_PER_GROWTH) pages = PAGES_PER_GROWTH;

	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) return false;

	uint64_t* pml4_virt = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	uintptr_t grow_vaddr = heap_current_top;

	for (size_t i = 0; i < pages; i++) {
		vmm_map_page(pml4_virt, grow_vaddr + (i * 4096), phys + (i * 4096), KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_BLOCK);
	}

	BlockHeader *curr = head;
	while (curr->next) curr = curr->next;

	if (curr->is_free && !curr->is_pool_block) {
		curr->size += (pages * 4096);
	} else {
		BlockHeader *new_block = (BlockHeader*)grow_vaddr;
		new_block->magic = HEAP_MAGIC;
		new_block->size = (pages * 4096) - sizeof(BlockHeader);
		new_block->is_free = true;
		new_block->is_pool_block = false;
		new_block->prev = curr;
		new_block->next = NULL;
		curr->next = new_block;
	}

	heap_current_top += (pages * 4096);
	return true;
}

static void heap_init_pools(void) {
	uint64_t* pml4_v = (uint64_t*)(vmm_get_pml4() + hhdm_offset);

	for (int i = 0; i < POOL_COUNT; i++) {
		pools[i].block_size = pool_sizes[i];
		pools[i].free_list = NULL;

		uintptr_t phys = pma_alloc_page();
		if (!phys) continue;

		uintptr_t virt = phys + hhdm_offset;

		vmm_map_page(pml4_v, virt, phys, KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_POOL);

		if (registered_pool_pages < MAX_POOL_PAGES) {
			pool_page_registry[registered_pool_pages++] = virt;
		}

		BlockHeader* bh = (BlockHeader*)virt;
		bh->magic = HEAP_MAGIC;
		bh->is_pool_block = true;
		bh->is_free = false;
		bh->bin_size = pool_sizes[i];
		bh->size = 4096 - sizeof(BlockHeader);
		bh->used_slots = 0;

		// link to main heap
		BlockHeader* curr = head;
		while(curr->next) curr = curr->next;
		curr->next = bh;
		bh->prev = curr;
		bh->next = NULL;

		uint8_t* data_start = (uint8_t*)(bh + 1);
		for (size_t j = 0; j <= (bh->size - pool_sizes[i]); j += pool_sizes[i]) {
			PoolNode* node = (PoolNode*)(data_start + j);
			node->next = pools[i].free_list;
			pools[i].free_list = node;
		}
	}
}

void heap_init(void* start_address, size_t total_size) {
	size_t pages = (total_size + 4095) / 4096; 	// determine how many physical pages we need

	// allocate phys
	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) panic("HEAP INIT", "Could not allocate physical memory for heap");

	// map virt to phys
	uintptr_t vaddr = (uintptr_t)start_address;
	uintptr_t paddr = phys;

	// map each page
	uint64_t* pml4_virt = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	for (size_t i = 0; i < pages; i++) {
		vmm_map_page(pml4_virt, vaddr, paddr, KERNEL_HEAP_FLAGS | PTE_TYPE_HEAP_BLOCK);
		vaddr += 4096;
		paddr += 4096;
	}

	head = (BlockHeader*)start_address;
	head->magic = HEAP_MAGIC;
	head->size = (pages * 4096) - sizeof(BlockHeader);
	head->is_free = true;
	head->is_pool_block = false;
	head->next = NULL;
	head->prev = NULL;

	heap_current_top = vaddr;
	heap_init_pools();
}

void* malloc(size_t size) {
	if (size == 0) return NULL;

	for (int i = 0; i < POOL_COUNT; i++) {
		if (size <= pools[i].block_size && pools[i].free_list) {
			void* ptr = pools[i].free_list;
			pools[i].free_list = pools[i].free_list->next;

			BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
			page_header->used_slots++;
			return ptr;
		}
	}

	size = ALIGN(size);
	retry:;
	BlockHeader *curr = last_fit ? last_fit : head;
	BlockHeader *start = curr;
	bool wrapped = false;

	while (curr) {
		if (curr->magic != HEAP_MAGIC) panic("MEMORY CORRUPTION", "Heap corruption!");

		if (curr->is_free && curr->size >= size) {
			if (curr->size >= (size + sizeof(BlockHeader) + MIN_SPLIT_SIZE)) {
				BlockHeader *next_block = (BlockHeader*)((uint8_t*)(curr + 1) + size);
				next_block->magic = HEAP_MAGIC;
				next_block->size = curr->size - size - sizeof(BlockHeader);
				next_block->is_free = true;
				next_block->is_pool_block = false;
				next_block->next = curr->next;
				next_block->prev = curr;
				if (curr->next) curr->next->prev = next_block;
				curr->next = next_block;
				curr->size = size;
			}
			curr->is_free = false;
			last_fit = curr;

			return (void*)(curr + 1);
		}
		curr = curr->next;
		if (curr == NULL && !wrapped) { curr = head; wrapped = true; }
		if (curr == start && wrapped) break;
	}

	if (heap_grow(size)) goto retry;
	panic("MEMORY EXHAUSTION", "the heap is out of memory (OOM)");
	return NULL;
}

static void _free(void* ptr, uint8_t type) {
	if (!ptr) return;
	if (type == PTE_STATE_HEAP_POOL) {
		BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
		size_t bin = page_header->bin_size;

		for (int i = 0; i < POOL_COUNT; i++) {
			if (pools[i].block_size == bin) {
				if (page_header->used_slots > 0) {
					page_header->used_slots--;
				}

				PoolNode* node = (PoolNode*)ptr;
				node->next = pools[i].free_list;
				pools[i].free_list = node;

				return;
			}
		}
		panic("MEMORY CORRUPTION", "Pool block found but bin mismatch!");

	} else if (type == PTE_STATE_HEAP_BLOCK) {
		BlockHeader *block = ((BlockHeader*)ptr) - 1;

		if (block->magic != HEAP_MAGIC) {
			panic("MEMORY CORRUPTION", "Heap header magic mismatch!");
		}

		block->is_free = true;

		// Forward Coalesce:
		if (block->next && block->next->is_free && !block->next->is_pool_block) {
			block->size += sizeof(BlockHeader) + block->next->size;
			block->next = block->next->next;
			if (block->next) block->next->prev = block;
		}

		// Backward Coalesce:
		if (block->prev && block->prev->is_free && !block->prev->is_pool_block) {
			BlockHeader *p = block->prev;
			p->size += sizeof(BlockHeader) + block->size;
			p->next = block->next;
			if (block->next) block->next->prev = p;
		}

	}
}

void* realloc(void* ptr, size_t new_size) {
	if (!ptr) return malloc(new_size);
	if (new_size == 0) { free(ptr); return NULL; }

	new_size = ALIGN(new_size);

	uint64_t* pml4_v = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	uint64_t* pte = vmm_get_pte(pml4_v, (uintptr_t)ptr, 0);

	if (!pte || !(*pte & 1)) panic("MEMORY CORRUPTION", "Attempted to realloc non heap memory");;

	uint8_t type = (uint8_t)((*pte >> 9) & 0x07);
	size_t old_size = 0;

	if (type == PTE_STATE_HEAP_POOL) {
		BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
		old_size = page_header->bin_size;

		if (new_size <= old_size) return ptr;

		void* new_p = malloc(new_size);
		if (new_p) {
			memcpy(new_p, ptr, old_size);
			free(ptr);
		}
		return new_p;
	}

	if (type == PTE_STATE_HEAP_BLOCK) {
		BlockHeader *curr = (BlockHeader*)ptr - 1;
		if (curr->magic != HEAP_MAGIC) panic("MEMORY CORRUPTION", "realloc on invalid magic!");

		old_size = curr->size;
		if (old_size >= new_size) return ptr;

		// try in place expansion
		if (curr->next && curr->next->is_free && !curr->next->is_pool_block &&
			(curr->size + sizeof(BlockHeader) + curr->next->size) >= new_size) {

			size_t total_avail = curr->size + sizeof(BlockHeader) + curr->next->size;

			if (total_avail >= (new_size + sizeof(BlockHeader) + MIN_SPLIT_SIZE)) {
				size_t leftover = total_avail - new_size - sizeof(BlockHeader);
				curr->size = new_size;
				BlockHeader *split = (BlockHeader*)((uint8_t*)(curr + 1) + new_size);
				split->magic = HEAP_MAGIC;
				split->size = leftover;
				split->is_free = true;
				split->is_pool_block = false;
				split->next = curr->next->next;
				split->prev = curr;
				if (split->next) split->next->prev = split;
				curr->next = split;
			} else {
				curr->size = total_avail;
				curr->next = curr->next->next;
				if (curr->next) curr->next->prev = curr;
			}
			return ptr;
		}

		// migration required
		void* new_ptr = malloc(new_size);
		if (new_ptr) {
			memcpy(new_ptr, ptr, old_size);
			free(ptr);
		}
		return new_ptr;
	}

	panic("REALLOC ERROR", "Attempted to realloc non heap memory");
	return NULL;
}

void free(void* ptr) {
	uint64_t* pte;
	uint8_t type = get_ptr_type(ptr, &pte);
	if (type != PTE_STATE_NOINFO) _free(ptr, type);
}

void* zalloc(size_t size) {
	void* p = malloc(size);
	if(p)
		memset(p, 0, size);
	return p;
}

void zfree(void* ptr) {
	if(!ptr) return;

	uint64_t* pte;
	uint8_t type = get_ptr_type(ptr, &pte);
	size_t size = 0;

	if (type == PTE_STATE_HEAP_POOL) {
		size = ((BlockHeader*)((uintptr_t)ptr & ~0xFFF))->bin_size;
	} else if (type == PTE_STATE_HEAP_BLOCK) {
		size = (((BlockHeader*)ptr) - 1)->size;
	}

	if (size > 0) memset(ptr, 0, size);
	free(ptr);
}

void sfree(void* ptr, size_t size) {
	UNUSED(size);
	free(ptr);
}

void szfree(void* ptr, size_t size) {
	UNUSED(size);
	zfree(ptr);
}

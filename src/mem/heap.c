#include "heap.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "mem.h" // IWYU pragma: keep
#include <limine.h>
#include "pma.h"
#include "kstate.h"
#include "stdio.h"
#include "logbuf.h"

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEAP_MAGIC 0xCAFEBABE
#define MIN_SPLIT_SIZE 16
#define PAGES_PER_GROWTH 4

// Pool configuration for small objects
#define POOL_COUNT 3
static size_t pool_sizes[POOL_COUNT] = { 32, 64, 128 };

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

static bool is_pool_ptr(void* ptr) {
	uintptr_t page_base = (uintptr_t)ptr & ~0xFFF;
	for (int i = 0; i < registered_pool_pages; i++) {
		if (pool_page_registry[i] == page_base) return true;
	}
	return false;
}

static size_t get_safe_size(void* ptr) {
	if (!ptr) return 0;
	if (is_pool_ptr(ptr)) {
		BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
		return page_header->bin_size;
	}
	BlockHeader *bh = (BlockHeader*)ptr - 1;
	return (bh->magic == HEAP_MAGIC) ? bh->size : 0;
}

void panic(const char* header_msg, const char* msg);

static bool heap_grow(size_t size_needed) {
	size_t total = size_needed + sizeof(BlockHeader);
	size_t pages = (total + 4095) / 4096;
	if (pages < PAGES_PER_GROWTH) pages = PAGES_PER_GROWTH;

	uintptr_t phys = pma_alloc_pages(pages);
	if (!phys) return false;

	BlockHeader *new_block = (BlockHeader*)(phys + hhdm_offset);
	new_block->magic = HEAP_MAGIC;
	new_block->size = (pages * 4096) - sizeof(BlockHeader);
	new_block->is_free = true;
	new_block->is_pool_block = false;

	BlockHeader *curr = head;
	while (curr->next) curr = curr->next;
	curr->next = new_block;
	new_block->prev = curr;
	new_block->next = NULL;

	if (curr->is_free && !curr->is_pool_block) {
		curr->size += sizeof(BlockHeader) + new_block->size;
		curr->next = NULL;
	}
	return true;
}

static void heap_init_pools(void) {
	for (int i = 0; i < POOL_COUNT; i++) {
		pools[i].block_size = pool_sizes[i];
		pools[i].free_list = NULL;

		uintptr_t phys = pma_alloc_page();
		if (!phys) continue;

		uintptr_t virt = phys + hhdm_offset;

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
	head = (BlockHeader*)start_address;
	head->magic = HEAP_MAGIC;
	head->size = total_size - sizeof(BlockHeader);
	head->is_free = true;
	head->is_pool_block = false;
	head->next = NULL;
	head->prev = NULL;
	heap_init_pools();
}

void* malloc(size_t size) {
	if (size == 0) return NULL;

	for (int i = 0; i < POOL_COUNT; i++) {
		if (size <= pools[i].block_size && pools[i].free_list) {
			void* ptr = pools[i].free_list;
			pools[i].free_list = pools[i].free_list->next;
			if (global_kernel_config.debug == 1) {
				logbuf_vwrite(LOG_LEVEL_DEBUG, "[MALLOC] Pool hit (");
				logbuf_vputhex64(LOG_LEVEL_DEBUG, pools[i].block_size);
				logbuf_vwrite(LOG_LEVEL_DEBUG, " bin) at ");
				logbuf_vputhex64(LOG_LEVEL_DEBUG, (uint64_t)ptr);
				logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
			}
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
			if (global_kernel_config.debug == 1) {
				logbuf_vwrite(LOG_LEVEL_DEBUG, "[MALLOC] Allocated ");
				logbuf_vputhex64(LOG_LEVEL_DEBUG, size);
				logbuf_vwrite(LOG_LEVEL_DEBUG, " bytes at ");
				logbuf_vputhex64(LOG_LEVEL_DEBUG, (uint64_t)(curr + 1));
				logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
			}
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

void free(void* ptr) {
	if (!ptr) return;

	if (is_pool_ptr(ptr)) {
		BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
		size_t bin = page_header->bin_size;

		for (int i = 0; i < POOL_COUNT; i++) {
			if (pools[i].block_size == bin) {
				if (page_header->used_slots > 0) {
					page_header->used_slots--;
				} else {
					if (global_kernel_config.debug == 1) {
						logbuf_vwrite(LOG_LEVEL_DEBUG, "[WARN] Double free or Underflow on bin: ");
						logbuf_vputhex64(LOG_LEVEL_DEBUG, bin);
						logbuf_vwrite(LOG_LEVEL_DEBUG, " at ");
						logbuf_vputhex64(LOG_LEVEL_DEBUG, (uintptr_t)ptr);
						logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
					}
					return;
				}

				PoolNode* node = (PoolNode*)ptr;
				node->next = pools[i].free_list;
				pools[i].free_list = node;

				if (global_kernel_config.debug == 1) {
					logbuf_vwrite(LOG_LEVEL_DEBUG, "[FREE] Pool block (");
					logbuf_vputhex64(LOG_LEVEL_DEBUG, bin);
					logbuf_vwrite(LOG_LEVEL_DEBUG, " bin) returned.\n");
				}
				return;
			}
		}
		panic("MEMORY CORRUPTION", "Pool block found but bin mismatch!");
	}

	BlockHeader *block = ((BlockHeader*)ptr) - 1;
	if (block->magic != HEAP_MAGIC) {
		logbuf_vwrite(LOG_LEVEL_DEBUG, "Invalid free at: ");
		logbuf_vputhex64(LOG_LEVEL_DEBUG, (uintptr_t)ptr);
		panic("MEMORY CORRUPTION", "\nHeap corruption or invalid pointer!");
	}

	if (global_kernel_config.debug == 1) {
		logbuf_vwrite(LOG_LEVEL_DEBUG, "[FREE] Reclaiming heap block (Size: ");
		logbuf_vputhex64(LOG_LEVEL_DEBUG, block->size);
		logbuf_vwrite(LOG_LEVEL_DEBUG, ")\n");
	}

	block->is_free = true;

	if (block->next && block->next->is_free && !block->next->is_pool_block) {
		block->size += sizeof(BlockHeader) + block->next->size;
		block->next = block->next->next;
		if (block->next) block->next->prev = block;
	}

	if (block->prev && block->prev->is_free && !block->prev->is_pool_block) {
		BlockHeader *p = block->prev;
		p->size += sizeof(BlockHeader) + block->size;
		p->next = block->next;
		if (block->next) block->next->prev = p;
	}
}

void* realloc(void* ptr, size_t new_size) {
	if (!ptr) return malloc(new_size);
	if (new_size == 0) { free(ptr); return NULL; }

	new_size = ALIGN(new_size);
	size_t old_size = get_safe_size(ptr);

	if (is_pool_ptr(ptr)) {
		if (new_size <= old_size) return ptr;

		if (global_kernel_config.debug == 1) {
			logbuf_vwrite(LOG_LEVEL_DEBUG, "[REALLOC] Pool migration: ");
			logbuf_vputhex64(LOG_LEVEL_DEBUG, old_size);
			logbuf_vwrite(LOG_LEVEL_DEBUG, " -> ");
			logbuf_vputhex64(LOG_LEVEL_DEBUG, new_size);
			logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
		}

		void* new_p = malloc(new_size);
		if (new_p) {
			memcpy(new_p, ptr, old_size);
			free(ptr);
		}
		return new_p;
	}

	BlockHeader *curr = (BlockHeader*)ptr - 1;
	if (curr->magic != HEAP_MAGIC) panic("MEMORY CORRUPTION", "Realloc on invalid pointer!");

	if (curr->size >= new_size) return ptr;

	if (curr->next && curr->next->is_free && !curr->next->is_pool_block &&
		(curr->size + sizeof(BlockHeader) + curr->next->size) >= new_size) {

		if (global_kernel_config.debug == 1) logbuf_vwrite(LOG_LEVEL_DEBUG, "[REALLOC] In-place expansion\n");

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

	if (global_kernel_config.debug == 1) logbuf_vwrite(LOG_LEVEL_DEBUG, "[REALLOC] Heap migration required\n");
	void* new_ptr = malloc(new_size);
	if (new_ptr) {
		memcpy(new_ptr, ptr, old_size);
		free(ptr);
	}
	return new_ptr;
}

void* zalloc(size_t size) {
	void* p = malloc(size);
	if(p)
		memset(p, 0, size);
	return p;
}

void zfree(void* ptr) {
	if(!ptr) return;
	size_t size = get_safe_size(ptr);
	if (size > 0) {
		memset(ptr, 0, size);
	}
	free(ptr);
}

void sfree(void* ptr, size_t size) {
	UNUSED(size);
	free(ptr);
}

void szfree(void* ptr, size_t size) {
	UNUSED(size);
	if(!ptr)
		return;
	BlockHeader* bh = (BlockHeader*)ptr - 1;
	memset(ptr, 0, bh->size);
	free(ptr);
}

void dump_memory_stats(void) {
	size_t total_managed = 0;
	size_t total_user_data = 0;
	size_t total_metadata = 0;
	size_t total_free_memory = 0;

	BlockHeader *curr = head;
	while (curr) {
		if (curr->magic != HEAP_MAGIC) {
			panic("MEMORY CORRUPTION", "HEAP CORRUPTION\n");
			break;
		}

		total_metadata += sizeof(BlockHeader);
		size_t block_total = sizeof(BlockHeader) + curr->size;
		total_managed += block_total;

		if (curr->is_pool_block) {
			size_t max_slots = curr->size / curr->bin_size;
			size_t used_slots = (curr->used_slots > max_slots) ? max_slots : curr->used_slots;

			size_t used_bytes = used_slots * curr->bin_size;
			total_user_data += used_bytes;
			total_free_memory += (curr->size - used_bytes);
		} else {
			if (curr->is_free) {
				total_free_memory += curr->size;
			} else {
				total_user_data += curr->size;
			}
		}
		curr = curr->next;
	}

	logbuf_write("\n[HEAP] Total Managed:  "); logbuf_puthex64(total_managed);   logbuf_putc('\n');
	logbuf_write("[HEAP] User Allocated: "); logbuf_puthex64(total_user_data);  logbuf_putc('\n');
	logbuf_write("[HEAP] Free Available: "); logbuf_puthex64(total_free_memory); logbuf_putc('\n');
	logbuf_write("[HEAP] Bookkeeping:	"); logbuf_puthex64(total_metadata);   logbuf_write("\n");

	size_t total_p = pma_get_total_pages();
	size_t free_p  = pma_get_free_pages();
	size_t used_p  = total_p - free_p;

	logbuf_vwrite(LOG_LEVEL_DEBUG, "[PMA]  Total pages:	"); logbuf_vputhex64(LOG_LEVEL_DEBUG, total_p); logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
	logbuf_vwrite(LOG_LEVEL_DEBUG, "[PMA]  Free pages:	 "); logbuf_vputhex64(LOG_LEVEL_DEBUG, free_p); logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
	logbuf_vwrite(LOG_LEVEL_DEBUG, "[PMA]  Used pages:	 "); logbuf_vputhex64(LOG_LEVEL_DEBUG, used_p); logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
}

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "mem.h"
#include <limine.h>
#include "pma.h"
#include "kstate.h"
#include "stdio.h"
#include "logbuf.h"

#define ALIGNMENT 8
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
    size_t size;
    bool is_free;
    bool is_pool_block; // prevents coalescing pool chunks into main heap
    size_t bin_size;
    uint32_t used_slots;
    struct BlockHeader *next;
    struct BlockHeader *prev;
} BlockHeader;

static BlockHeader *head = NULL;
static BlockHeader *last_fit = NULL;
static FixedPool pools[POOL_COUNT];

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

void heap_init_pools() {
    for (int i = 0; i < POOL_COUNT; i++) {
        pools[i].block_size = pool_sizes[i];
        pools[i].free_list = NULL;
        
        // one page per pool to start
        uintptr_t phys = pma_alloc_page();
        if (!phys) continue;

        uint8_t* ptr = (uint8_t*)(phys + hhdm_offset);

        BlockHeader* bh = (BlockHeader*)ptr;
        bh->magic = HEAP_MAGIC;
        bh->is_pool_block = true;
        bh->is_free = false; 
        bh->bin_size = pool_sizes[i];
        bh->size = 4096 - sizeof(BlockHeader);
        
        // link to main heap so we dont lose track of page
        BlockHeader* curr = head;
        while(curr->next) curr = curr->next;
        curr->next = bh;
        bh->prev = curr;
        bh->next = NULL;

        // divide the page into chunks
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
                logbuf_vputhex(LOG_LEVEL_DEBUG, pools[i].block_size);
                logbuf_vwrite(LOG_LEVEL_DEBUG, " bin) at ");
                logbuf_vputhex(LOG_LEVEL_DEBUG, (uint64_t)ptr);
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
                logbuf_vputhex(LOG_LEVEL_DEBUG, size);
                logbuf_vwrite(LOG_LEVEL_DEBUG, " bytes at ");
                logbuf_vputhex(LOG_LEVEL_DEBUG, (uint64_t)(curr + 1));
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

    BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);

    if (page_header->magic == HEAP_MAGIC && page_header->is_pool_block) {
        size_t bin = page_header->bin_size;

        for (int i = 0; i < POOL_COUNT; i++) {
            if (pools[i].block_size == bin) {
                PoolNode* node = (PoolNode*)ptr;
                void* caller = __builtin_return_address(0);

                if (page_header->used_slots > 0) {
                    page_header->used_slots--;
                } else {
                    if (global_kernel_config.debug == 1) {
                        logbuf_vwrite(LOG_LEVEL_DEBUG, "[WARN] Double free or Underflow on bin: ");
                        logbuf_vputhex(LOG_LEVEL_DEBUG, bin);
                        logbuf_vwrite(LOG_LEVEL_DEBUG, " at ");
                        logbuf_vputhex(LOG_LEVEL_DEBUG, (uintptr_t)ptr);
                        logbuf_vwrite(LOG_LEVEL_DEBUG, " from return address: ");
                        logbuf_vputhex(LOG_LEVEL_DEBUG, (uintptr_t)caller);
                        logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
                    }
                    return;
                }

                node->next = pools[i].free_list;
                pools[i].free_list = node;

                if (global_kernel_config.debug == 1) {
                    logbuf_vwrite(LOG_LEVEL_DEBUG, "[FREE] Pool block (");
                    logbuf_vputhex(LOG_LEVEL_DEBUG, bin);
                    logbuf_vwrite(LOG_LEVEL_DEBUG, " bin) returned.\n");
                }
                return;
            }
        }
        panic("MEMORY CORRUPTION", "Pool block found but bin size mismatch!");
    }

    BlockHeader *block = ((BlockHeader*)ptr) - 1;
    if (block->magic != HEAP_MAGIC) {
        logbuf_vwrite(LOG_LEVEL_DEBUG, "Invalid free at: ");
        logbuf_vputhex(LOG_LEVEL_DEBUG, (uintptr_t)ptr);
        panic("MEMORY CORRUPTION", "\nHeap corruption or invalid pointer!");
    }

    if (global_kernel_config.debug == 1) {
        logbuf_vwrite(LOG_LEVEL_DEBUG, "[FREE] Reclaiming heap block (Size: ");
        logbuf_vputhex(LOG_LEVEL_DEBUG, block->size);
        logbuf_vwrite(LOG_LEVEL_DEBUG, ")\n");
    }

    block->is_free = true;

    // Coalesce forward
    if (block->next && block->next->is_free && !block->next->is_pool_block) {
        block->size += sizeof(BlockHeader) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }

    // Coalesce backward
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

    BlockHeader *page_header = (BlockHeader*)((uintptr_t)ptr & ~0xFFF);
    
    if (page_header->magic == HEAP_MAGIC && page_header->is_pool_block) {
        size_t old_size = page_header->bin_size;

        if (new_size <= old_size) return ptr;

        if (global_kernel_config.debug == 1) {
            logbuf_vwrite(LOG_LEVEL_DEBUG, "[REALLOC] Pool migration: ");
            logbuf_vputhex(LOG_LEVEL_DEBUG, old_size);
            logbuf_vwrite(LOG_LEVEL_DEBUG, " -> ");
            logbuf_vputhex(LOG_LEVEL_DEBUG, new_size);
            logbuf_vwrite(LOG_LEVEL_DEBUG, "\n");
        }

        void* new_p = malloc(new_size);
        if (new_p) {
            memcpy(new_p, ptr, old_size);
            free(ptr);
            ptr = NULL;
        }
        return new_p;
    }

    BlockHeader *curr = (BlockHeader*)ptr - 1;
    if (curr->magic != HEAP_MAGIC) panic("MEMORY CORRUPTION", "Realloc on invalid pointer!");

    if (curr->size >= new_size) return ptr;

    if (curr->next && curr->next->is_free && !curr->next->is_pool_block &&
        (curr->size + sizeof(BlockHeader) + curr->next->size) >= new_size) {
        
        size_t total_avail = curr->size + sizeof(BlockHeader) + curr->next->size;
        if (global_kernel_config.debug == 1) logbuf_vwrite(LOG_LEVEL_DEBUG, "[REALLOC] In-place expansion\n");

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
        memcpy(new_ptr, ptr, curr->size);
        free(ptr);
        ptr = NULL;
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
    if(!ptr) 
        return; 
    BlockHeader* bh = (BlockHeader*)ptr - 1; 
    memset(ptr, 0, bh->size); 
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

void heap_dump_stats(void) {
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

    logbuf_write("\n--- CUOREOS HEAP STATS ---\n");
    logbuf_write("Total Managed:  "); logbuf_puthex(total_managed);   logbuf_putc('\n');
    logbuf_write("User Allocated: "); logbuf_puthex(total_user_data);  logbuf_putc('\n');
    logbuf_write("Free Available: "); logbuf_puthex(total_free_memory); logbuf_putc('\n');
    logbuf_write("Bookkeeping:    "); logbuf_puthex(total_metadata);   logbuf_write("\n");
    logbuf_write("--------------------------\n\n");
}
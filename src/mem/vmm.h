#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "datatype/avl.h"

#define VMM_MAX_ALLOCATIONS 4096

typedef struct VMMNode {
	avl_node_t node;

	uintptr_t start;
	size_t page_count;
	size_t max_gap;
} VMMNode;

void vmm_init(void);
uintptr_t vmm_alloc_pages(size_t count);
uintptr_t vmm_free_pages(uintptr_t virt, size_t count);

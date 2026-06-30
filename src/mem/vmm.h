#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "datatype/avl.h"

typedef struct VMMNode {
	avl_node_t node;

	uintptr_t start;
	size_t page_count;
	size_t max_gap;
	uintptr_t subtree_start;
	uintptr_t subtree_end;
	bool heap_allocated;
} VMMNode;

void vmm_init(void);
void vmm_switch_to_heap(void);
uintptr_t vmm_alloc_pages(size_t count);
uintptr_t vmm_alloc_pages_2mb(size_t count);
uintptr_t vmm_alloc_aligned(size_t count, size_t align);
uintptr_t vmm_free_pages(uintptr_t virt, size_t count);

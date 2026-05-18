#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VMM_MAX_ALLOCATIONS 4096

typedef struct VMMNode {
	uintptr_t start;
	size_t page_count;

	int height;
	size_t max_gap;

	uintptr_t subtree_min;
	uintptr_t subtree_max;

	struct VMMNode *left;
	struct VMMNode *right;
} VMMNode;

typedef struct {
	size_t pages_needed;
	uintptr_t candidate_addr;
	uintptr_t pool_end;
	bool found;
} VMMTreeSearch;

void vmm_init(void);
uintptr_t vmm_alloc_pages(size_t count);
uintptr_t vmm_free_pages(uintptr_t virt, size_t count);

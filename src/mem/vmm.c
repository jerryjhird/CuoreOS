#include "vmm.h"

#include "mem.h"
#include "cmm.h"
#include "logbuf.h"
#include "abs.h"
#include "panic.h"
#include "paging.h"
#include "datatype/avl.h"
#include "heap.h"

static uintptr_t vmm_pool_base = 0;
static uintptr_t vmm_pool_end = 0;
static VMMNode *vmm_root = NULL;

static inline uintptr_t align_up(uintptr_t addr, size_t align) {
	uintptr_t aligned = (addr + align - 1) & ~(uintptr_t)(align - 1);
	return (aligned < addr) ? addr : aligned;
}

// early-boot static pool, used before heap is available
#define VMM_EARLY_POOL 64
static VMMNode early_pool[VMM_EARLY_POOL];
static size_t early_pool_count = 0;
static bool vmm_heap_available = false;

#define VMM_MAX_GAP(n_ptr) ((n_ptr) ? GET_CONTAINER(n_ptr, VMMNode, node)->max_gap : 0)

static uintptr_t vmm_subtree_end(VMMNode *n) {
	return n ? n->subtree_end : 0;
}

static uintptr_t vmm_subtree_start(VMMNode *n) {
	return n ? n->subtree_start : (uintptr_t)-1;
}

static int vmm_node_cmp(avl_node_t *a, avl_node_t *b) {
	VMMNode *va = GET_CONTAINER(a, VMMNode, node);
	VMMNode *vb = GET_CONTAINER(b, VMMNode, node);

	if (va->start < vb->start) return -1;
	if (va->start > vb->start) return 1;
	return 0;
}

static void vmm_node_update(avl_node_t *n) {
	VMMNode *node = GET_CONTAINER(n, VMMNode, node);
	uintptr_t nend = node->start + node->page_count * PAGE_SIZE;

	// subtree_start from left child or self
	if (node->node.left) {
		VMMNode *left = GET_CONTAINER(node->node.left, VMMNode, node);
		node->subtree_start = left->subtree_start;
	} else {
		node->subtree_start = node->start;
	}

	// subtree_end from right child or self
	if (node->node.right) {
		VMMNode *right = GET_CONTAINER(node->node.right, VMMNode, node);
		node->subtree_end = (right->subtree_end > nend) ? right->subtree_end : nend;
	} else {
		node->subtree_end = nend;
	}

	size_t gap_il = 0;
	if (node->node.left) {
		VMMNode *left = GET_CONTAINER(node->node.left, VMMNode, node);
		if (node->start >= left->subtree_end)
			gap_il = (node->start - left->subtree_end) / PAGE_SIZE;
	}

	size_t gap_ir = 0;
	if (node->node.right) {
		VMMNode *right = GET_CONTAINER(node->node.right, VMMNode, node);
		if (right->subtree_start >= nend)
			gap_ir = (right->subtree_start - nend) / PAGE_SIZE;
	}

	node->max_gap = SMAX(
		SMAX(VMM_MAX_GAP(node->node.left), VMM_MAX_GAP(node->node.right)),
		SMAX(gap_il, gap_ir));
}

static const avl_callbacks_t vmm_avl_cb = { .compare = vmm_node_cmp, .update = vmm_node_update };

static VMMNode *pool_alloc_node(void) {
	VMMNode *n;

	if (vmm_heap_available) {
		n = malloc(sizeof(VMMNode));
		if (!n) panic("VMM", "malloc failed");
		n->heap_allocated = true;
	} else {
		if (early_pool_count >= VMM_EARLY_POOL) panic("VMM", "early pool exhausted");
		n = &early_pool[early_pool_count++];
		n->heap_allocated = false;
	}

	*n = (VMMNode){0};
	return n;
}

static void pool_free_node(VMMNode *node) {
	if (node->heap_allocated) {
		free(node);
	}
	// early pool nodes are not individually freed, pool is append-only during boot
}

void vmm_switch_to_heap(void) {
	vmm_heap_available = true;
}

static uintptr_t vmm_find_gap(VMMNode *n, uintptr_t base, size_t pages, size_t align) {
	if (!n) return 0;

	uintptr_t span = pages * PAGE_SIZE;
	uintptr_t sub_start = vmm_subtree_start(n);

	uintptr_t aligned_base = align_up(base, align);

	if (sub_start >= aligned_base + span) {
		return (aligned_base + span <= vmm_pool_end) ? aligned_base : 0;
	}
	if (n->max_gap < pages) return 0;

	if (n->node.left) {
		VMMNode *l = GET_CONTAINER(n->node.left, VMMNode, node);
		uintptr_t addr = vmm_find_gap(l, base, pages, align);
		if (addr) return addr;

		uintptr_t lend = vmm_subtree_end(l);
		if (lend > base) base = lend;
	}

	aligned_base = align_up(base, align);

	if (n->start >= aligned_base + span) {
		return (aligned_base + span <= vmm_pool_end) ? aligned_base : 0;
	}

	uintptr_t nend = n->start + n->page_count * PAGE_SIZE;
	if (nend > base) base = nend;

	if (n->node.right) {
		VMMNode *r = GET_CONTAINER(n->node.right, VMMNode, node);
		uintptr_t addr = vmm_find_gap(r, base, pages, align);
		if (addr) return addr;
	}

	return 0;
}

void vmm_init(void) {
	uintptr_t max_ram = cmm_get_ram_top_2mb_aligned();

	vmm_pool_base = hhdm_offset + max_ram;

	// upper limit depends on paging mode
	vmm_pool_end = (paging_get_level() == 5)
		? 0xFFFFFFFFFFE00000ULL
		: 0xFFFFF00000000000ULL;

	vmm_root = NULL;
	early_pool_count = 0;
	vmm_heap_available = false;

	logbuf_ok("[ VMM  ] Initialized VMM with pool base: %p\n", (void*)vmm_pool_base);
}

uintptr_t vmm_alloc_aligned(size_t count, size_t align) {
	if (UNLIKELY(count == 0)) return 0;

	uintptr_t addr = vmm_find_gap(vmm_root, vmm_pool_base, count, align);

	if (!addr) {
		uintptr_t tail = vmm_root ? vmm_subtree_end(vmm_root) : vmm_pool_base;
		tail = align_up(tail, align);
		if (tail < vmm_pool_base) tail = vmm_pool_base;
		addr = tail;
	}

	if (addr + count * PAGE_SIZE > vmm_pool_end) {
		panic("VMM", "out of virtual address space");
	}

	VMMNode *n = pool_alloc_node();
	n->start = addr;
	n->page_count = count;
	vmm_root = GET_CONTAINER(avl_insert(vmm_root ? &vmm_root->node : NULL, &n->node, &vmm_avl_cb), VMMNode, node);

	return addr;
}

uintptr_t vmm_alloc_pages(size_t count) {
	return vmm_alloc_aligned(count, PAGE_SIZE);
}

uintptr_t vmm_alloc_pages_2mb(size_t count) {
	return vmm_alloc_aligned(count, PAGE_SIZE_2MB);
}

uintptr_t vmm_free_pages(uintptr_t virt, size_t count) {
	if (UNLIKELY(virt == 0)) return 0;
	if (UNLIKELY(!vmm_root)) return 0;

	VMMNode key = { .start = virt };
	avl_node_t *out = NULL;
	avl_node_t *new_root = avl_delete(&vmm_root->node, &key.node, &vmm_avl_cb, &out);

	vmm_root = new_root ? GET_CONTAINER(new_root, VMMNode, node) : NULL;

	if (UNLIKELY(!out)) return 0;

	VMMNode *node = GET_CONTAINER(out, VMMNode, node);
	if (UNLIKELY(node->page_count != count)) {
		pool_free_node(node);
		return 0;
	}

	uintptr_t addr = node->start;
	pool_free_node(node);

	return addr;
}

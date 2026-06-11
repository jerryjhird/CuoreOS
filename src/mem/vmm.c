#include "vmm.h"

#include "mem.h"
#include "cmm.h"
#include "logbuf.h"
#include "abs.h"
#include "panic.h"
#include "datatype/avl.h"

static uintptr_t vmm_pool_base = 0;
static uintptr_t vmm_pool_end = 0;
static VMMNode *vmm_root = NULL;
static VMMNode node_pool[VMM_MAX_ALLOCATIONS];
static uint8_t node_bitmap[VMM_MAX_ALLOCATIONS / 8];
static size_t node_bitmap_hint = 0;

#define VMM_MAX_GAP(n_ptr) ((n_ptr) ? GET_CONTAINER(n_ptr, VMMNode, node)->max_gap : 0)

static uintptr_t vmm_subtree_end(VMMNode *n) {
	if (!n) { return 0; }

	avl_node_t *max = avl_get_max(&n->node);
	VMMNode *node = GET_CONTAINER(max, VMMNode, node);

	return node->start + node->page_count * PAGE_SIZE;
}

static uintptr_t vmm_subtree_start(VMMNode *n) {
	if (!n) { return (uintptr_t)-1; }

	avl_node_t *min = avl_get_min(&n->node);
	return GET_CONTAINER(min, VMMNode, node)->start;
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
	size_t gap_il = 0;

	if (node->node.left) {
		uintptr_t lend = vmm_subtree_end(GET_CONTAINER(node->node.left, VMMNode, node));
		if (node->start >= lend) { gap_il = (node->start - lend) / PAGE_SIZE; }
	}

	size_t gap_ir = 0;

	if (node->node.right) {
		uintptr_t nend = node->start + node->page_count * PAGE_SIZE;
		uintptr_t rstart = vmm_subtree_start(GET_CONTAINER(node->node.right, VMMNode, node));
		if (rstart >= nend) { gap_ir = (rstart - nend) / PAGE_SIZE; }
	}

	node->max_gap = SMAX(SMAX(VMM_MAX_GAP(node->node.left), VMM_MAX_GAP(node->node.right)),
						 SMAX(gap_il, gap_ir));
}

static const avl_callbacks_t vmm_avl_cb = { .compare = vmm_node_cmp, .update = vmm_node_update };

static VMMNode *pool_alloc_node(void) {
	for (size_t i = node_bitmap_hint; i < VMM_MAX_ALLOCATIONS; i++) {
		size_t byte = i / 8;
		size_t bit = i % 8;

		if (!(node_bitmap[byte] & (1u << bit))) {
			node_bitmap[byte] |= (1u << bit);
			node_bitmap_hint = i + 1;
			node_pool[i] = (VMMNode){0};
			return &node_pool[i];
		}
	}

	panic("VMM", "node pool exhausted");
	return NULL;
}

static void pool_free_node(VMMNode *node) {
	ptrdiff_t idx = node - node_pool;
	if (idx < 0 || idx >= VMM_MAX_ALLOCATIONS) { return; }

	size_t i = (size_t)idx;
	node_bitmap[i / 8] &= ~(1u << (i % 8));

	if (i < node_bitmap_hint) { node_bitmap_hint = i; }
}

static uintptr_t vmm_find_gap(VMMNode *n, uintptr_t base, size_t pages) {
	if (!n) { return 0; }
	uintptr_t span = pages * PAGE_SIZE;
	uintptr_t sub_start = vmm_subtree_start(n);

	if (sub_start >= base + span) { return base; }
	if (n->max_gap < pages) { return 0; }

	if (n->node.left) {
		VMMNode *l = GET_CONTAINER(n->node.left, VMMNode, node);
		uintptr_t addr = vmm_find_gap(l, base, pages);
		if (addr) { return addr; }
		uintptr_t lend = vmm_subtree_end(l);
		if (lend > base) { base = lend; }
	}

	if (n->start >= base + span) { return base; }
	uintptr_t nend = n->start + n->page_count * PAGE_SIZE;
	if (nend > base) { base = nend; }

	if (n->node.right) {
		VMMNode *r = GET_CONTAINER(n->node.right, VMMNode, node);
		uintptr_t addr = vmm_find_gap(r, base, pages);
		if (addr) { return addr; }
	}

	return 0;
}

void vmm_init(void) {
	uintptr_t max_phys = cmm_get_phys_top();

	max_phys = (max_phys + 0x1fffff) & ~(uintptr_t)0x1fffff;
	vmm_pool_base = hhdm_offset + max_phys;
	vmm_pool_end = 0xfffff00000000000ULL;
	vmm_root = NULL;
	memset(node_bitmap, 0, sizeof(node_bitmap));
	node_bitmap_hint = 0;

	logbuf_ok("[ VMM  ] Initialized VMM with pool base: %p\n", (void*)vmm_pool_base);
}

uintptr_t vmm_alloc_pages(size_t count) {
	if (UNLIKELY(count == 0)) { return 0; }
	uintptr_t addr = vmm_find_gap(vmm_root, vmm_pool_base, count);

	if (!addr) {
		uintptr_t tail = vmm_root ? vmm_subtree_end(vmm_root) : vmm_pool_base;
		if (tail < vmm_pool_base) { tail = vmm_pool_base; }
		addr = tail;
	}

	if (addr + count * PAGE_SIZE > vmm_pool_end) {
		panic("VMM", "out of virtual address space");
	}

	VMMNode *n = pool_alloc_node();
	n->start = addr;
	n->page_count = count;
	vmm_root = GET_CONTAINER(avl_insert(&vmm_root->node, &n->node, &vmm_avl_cb), VMMNode, node);

	return addr;
}

uintptr_t vmm_free_pages(uintptr_t virt, size_t count) {
	if (UNLIKELY(virt == 0)) { return 0; }

	VMMNode key = { .start = virt };
	avl_node_t *out = NULL;
	avl_node_t *new_root = avl_delete(&vmm_root->node, &key.node, &vmm_avl_cb, &out);

	vmm_root = new_root ? GET_CONTAINER(new_root, VMMNode, node) : NULL;

	if (UNLIKELY(!out)) { return 0; }

	VMMNode *node = GET_CONTAINER(out, VMMNode, node);
	if (UNLIKELY(node->page_count != count)) {
		panic("VMM", "free count mismatch");
	}

	uintptr_t addr = node->start;
	pool_free_node(node);

	return addr;
}

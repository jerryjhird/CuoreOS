#include "vmm.h"
#include "mem.h"
#include "cmm.h"
#include "logbuf.h"
#include "abs.h"
#include "panic.h"

static uintptr_t vmm_pool_base = 0;
static uintptr_t vmm_pool_end = 0;
static VMMNode *vmm_root = NULL;
static VMMNode node_pool[VMM_MAX_ALLOCATIONS];
static uint8_t node_bitmap[VMM_MAX_ALLOCATIONS / 8];
static size_t node_bitmap_hint = 0;

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

static inline int vmm_height(VMMNode *n) {
	return n ? n->height : 0;
}

static inline size_t vmm_max_gap(VMMNode *n) {
	return n ? n->max_gap : 0;
}

static inline int vmm_balance(VMMNode *n) {
	return n ? vmm_height(n->left) - vmm_height(n->right) : 0;
}

static inline int imax(int a, int b) {
	return a > b ? a : b;
}

static inline size_t smax(size_t a, size_t b) {
	return a > b ? a : b;
}

static uintptr_t vmm_subtree_end(VMMNode *n) {
	if (!n) { return 0; }
	while (n->right) { n = n->right; }
	return n->start + n->page_count * PAGE_SIZE;
}

static uintptr_t vmm_subtree_start(VMMNode *n) {
	if (!n) { return (uintptr_t)-1; }
	while (n->left) { n = n->left; }
	return n->start;
}

static void vmm_update(VMMNode *n) {
	if (!n) { return; }
	n->height = 1 + imax(vmm_height(n->left), vmm_height(n->right));
	size_t gap_il = 0;

	if (n->left) {
		uintptr_t lend = vmm_subtree_end(n->left);
		if (n->start >= lend) { gap_il = (n->start - lend) / PAGE_SIZE; }
	}

	size_t gap_ir = 0;

	if (n->right) {
		uintptr_t nend = n->start + n->page_count * PAGE_SIZE;
		uintptr_t rstart = vmm_subtree_start(n->right);
		if (rstart >= nend) { gap_ir = (rstart - nend) / PAGE_SIZE; }
	}

	n->max_gap = smax(smax(vmm_max_gap(n->left), vmm_max_gap(n->right)),
					  smax(gap_il, gap_ir));
}

static VMMNode *vmm_rot_r(VMMNode *y) {
	VMMNode *x = y->left;
	VMMNode *t = x->right;
	x->right = y;
	y->left = t;
	vmm_update(y);
	vmm_update(x);
	return x;
}

static VMMNode *vmm_rot_l(VMMNode *x) {
	VMMNode *y = x->right;
	VMMNode *t = y->left;
	y->left = x;
	x->right = t;
	vmm_update(x);
	vmm_update(y);
	return y;
}

static VMMNode *vmm_rebalance(VMMNode *n) {
	vmm_update(n);
	int bal = vmm_balance(n);

	if (bal > 1) {
		if (vmm_balance(n->left) < 0) { n->left = vmm_rot_l(n->left); }
		return vmm_rot_r(n);
	}

	if (bal < -1) {
		if (vmm_balance(n->right) > 0) { n->right = vmm_rot_r(n->right); }
		return vmm_rot_l(n);
	}

	return n;
}

static uintptr_t vmm_find_gap(VMMNode *n, uintptr_t base, size_t pages) {
	if (!n) { return 0; }
	uintptr_t span = pages * PAGE_SIZE;
	uintptr_t sub_start = vmm_subtree_start(n);

	if (sub_start >= base + span) { return base; }
	if (n->max_gap < pages) { return 0; }

	if (n->left) {
		uintptr_t addr = vmm_find_gap(n->left, base, pages);
		if (addr) { return addr; }
		uintptr_t lend = vmm_subtree_end(n->left);
		if (lend > base) { base = lend; }
	}

	if (n->start >= base + span) { return base; }
	uintptr_t nend = n->start + n->page_count * PAGE_SIZE;
	if (nend > base) { base = nend; }

	if (n->right) {
		uintptr_t addr = vmm_find_gap(n->right, base, pages);
		if (addr) { return addr; }
	}

	return 0;
}

static VMMNode *vmm_insert(VMMNode *node, VMMNode *n) {
	if (!node) { return n; }

	if (n->start < node->start) {
		node->left = vmm_insert(node->left, n);
	} else if (n->start > node->start) {
		node->right = vmm_insert(node->right, n);
	} else {
		panic("VMM", "overlapping virtual node");
	}

	return vmm_rebalance(node);
}

static VMMNode *vmm_unlink_leftmost(VMMNode *n, VMMNode **out) {
	if (!n->left) {
		*out = n;
		return n->right;
	}

	n->left = vmm_unlink_leftmost(n->left, out);
	return vmm_rebalance(n);
}

static VMMNode *vmm_delete(VMMNode *root, uintptr_t start, VMMNode **out) {
	if (!root) { return NULL; }
	if (start < root->start) {
		root->left = vmm_delete(root->left, start, out);
		return vmm_rebalance(root);
	}

	if (start > root->start) {
		root->right = vmm_delete(root->right, start, out);
		return vmm_rebalance(root);
	}

	*out = root;
	if (!root->left) { return root->right; }
	if (!root->right) { return root->left; }

	VMMNode *succ;
	VMMNode *new_right = vmm_unlink_leftmost(root->right, &succ);

	succ->left = root->left;
	succ->right = new_right;

	return vmm_rebalance(succ);
}

void vmm_init(void) {
	uintptr_t max_phys = cmm_get_phys_top();

	max_phys = (max_phys + 0x1fffff) & ~(uintptr_t)0x1fffff;
	vmm_pool_base = hhdm_offset + max_phys;
	vmm_pool_end = 0xfffff00000000000ULL;
	vmm_root = NULL;
	memset(node_bitmap, 0, sizeof(node_bitmap));
	node_bitmap_hint = 0;

	logbuf_printf("[ VMM  ] Initialized VMM with pool base: %p\n", (void*)vmm_pool_base);
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
	n->height = 1;
	vmm_root = vmm_insert(vmm_root, n);
	return addr;
}

uintptr_t vmm_free_pages(uintptr_t virt, size_t count) {
	if (UNLIKELY(virt == 0)) { return 0; }
	VMMNode *node = NULL;
	vmm_root = vmm_delete(vmm_root, virt, &node);

	if (UNLIKELY(!node)) { return 0; }

	if (UNLIKELY(node->page_count != count)) {
		panic("VMM", "free count mismatch");
	}

	uintptr_t addr = node->start;
	pool_free_node(node);
	return addr;
}

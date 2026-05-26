#include "avl.h"

static void update_node(avl_node_t *n, const avl_callbacks_t *cb) {
	if (!n) return;
	int lh = AVL_GET_HEIGHT(n->left);
	int rh = AVL_GET_HEIGHT(n->right);
	n->height = 1 + (lh > rh ? lh : rh);
	if (cb && cb->update) cb->update(n);
}

static avl_node_t* rotate_right(avl_node_t *y, const avl_callbacks_t *cb) {
	avl_node_t *x = y->left;
	avl_node_t *t = x->right;
	x->right = y;
	y->left = t;
	update_node(y, cb);
	update_node(x, cb);
	return x;
}

static avl_node_t* rotate_left(avl_node_t *x, const avl_callbacks_t *cb) {
	avl_node_t *y = x->right;
	avl_node_t *t = y->left;
	y->left = x;
	x->right = t;
	update_node(x, cb);
	update_node(y, cb);
	return y;
}

static avl_node_t* rebalance(avl_node_t *n, const avl_callbacks_t *cb) {
	update_node(n, cb);
	int bal = AVL_GET_BALANCE(n);
	if (bal > 1) {
		if (AVL_GET_BALANCE(n->left) < 0) n->left = rotate_left(n->left, cb);
		return rotate_right(n, cb);
	}
	if (bal < -1) {
		if (AVL_GET_BALANCE(n->right) > 0) n->right = rotate_right(n->right, cb);
		return rotate_left(n, cb);
	}
	return n;
}

static avl_node_t* unlink_leftmost(avl_node_t *n, avl_node_t **out, const avl_callbacks_t *cb) {
	if (!n->left) {
		*out = n;
		return n->right;
	}
	n->left = unlink_leftmost(n->left, out, cb);
	return rebalance(n, cb);
}

avl_node_t* avl_insert(avl_node_t *root, avl_node_t *n, const avl_callbacks_t *cb) {
	if (!root) return n;
	int cmp = cb->compare(n, root);
	if (cmp < 0) root->left = avl_insert(root->left, n, cb);
	else if (cmp > 0) root->right = avl_insert(root->right, n, cb);
	else return root;
	return rebalance(root, cb);
}

avl_node_t* avl_delete(avl_node_t *root, avl_node_t *key, const avl_callbacks_t *cb, avl_node_t **out) {
	if (!root) return NULL;
	int cmp = cb->compare(key, root);
	if (cmp < 0) root->left = avl_delete(root->left, key, cb, out);
	else if (cmp > 0) root->right = avl_delete(root->right, key, cb, out);
	else {
		*out = root;
		if (!root->left) return root->right;
		if (!root->right) return root->left;
		avl_node_t *succ;
		avl_node_t *new_right = unlink_leftmost(root->right, &succ, cb);
		succ->left = root->left;
		succ->right = new_right;
		return rebalance(succ, cb);
	}
	return rebalance(root, cb);
}

avl_node_t* avl_find(avl_node_t *root, avl_node_t *key, const avl_callbacks_t *cb) {
	while (root) {
		int cmp = cb->compare(key, root);
		if (cmp < 0) root = root->left;
		else if (cmp > 0) root = root->right;
		else return root;
	}
	return NULL;
}

avl_node_t* avl_get_min(avl_node_t *root) {
	if (!root) return NULL;
	while (root->left) root = root->left;
	return root;
}

avl_node_t* avl_get_max(avl_node_t *root) {
	if (!root) return NULL;
	while (root->right) root = root->right;
	return root;
}

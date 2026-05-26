#pragma once
#include <stddef.h>

typedef struct avl_node {
	struct avl_node *left;
	struct avl_node *right;
	int height;
} avl_node_t;

typedef struct avl_callbacks {
	int (*compare)(avl_node_t *a, avl_node_t *b);
	void (*update)(avl_node_t *n);
} avl_callbacks_t;

#define AVL_GET_HEIGHT(n) ((n) ? (n)->height : 0)
#define AVL_GET_BALANCE(n) ((n) ? (AVL_GET_HEIGHT((n)->left) - AVL_GET_HEIGHT((n)->right)) : 0)

avl_node_t* avl_insert(avl_node_t *root, avl_node_t *node, const avl_callbacks_t *cb);
avl_node_t* avl_delete(avl_node_t *root, avl_node_t *key, const avl_callbacks_t *cb, avl_node_t **out);
avl_node_t* avl_find(avl_node_t *root, avl_node_t *key, const avl_callbacks_t *cb);

avl_node_t* avl_get_min(avl_node_t *root);
avl_node_t* avl_get_max(avl_node_t *root);

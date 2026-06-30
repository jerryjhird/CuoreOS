#pragma once

#include <stddef.h>
#include "abs.h"

typedef struct list {
	struct list *prev;
	struct list *next;
} list_t; // circular doubly-linked list

#define LIST_INIT(head) { .prev = &(head), .next = &(head) }

// init head to point to itself (empty list)
static inline void list_init(list_t *head) {
	head->prev = head;
	head->next = head;
}

// add node after head
static inline void list_add(list_t *head, list_t *node) {
	node->next = head->next;
	node->prev = head;
	head->next->prev = node;
	head->next = node;
}

// add node before head (tail of the list)
static inline void list_add_tail(list_t *head, list_t *node) {
	node->prev = head->prev;
	node->next = head;
	head->prev->next = node;
	head->prev = node;
}

// remove node from whichever list it is in
static inline void list_del(list_t *node) {
	node->prev->next = node->next;
	node->next->prev = node->prev;
	node->prev = NULL;
	node->next = NULL;
}

static inline int list_is_empty(list_t *head) {
	return head->next == head;
}

#define list_entry(ptr, type, member) GET_CONTAINER(ptr, type, member)

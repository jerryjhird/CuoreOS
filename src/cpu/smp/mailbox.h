#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*mailbox_func_t)(void);

typedef struct {
	mailbox_func_t func;
	void *data;
	volatile bool pending;
} mailbox_t;

void mailbox_send(uint8_t cpu_id, mailbox_func_t func, void *data);
void mailman_send(mailbox_func_t func, void *data);

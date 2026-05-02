#pragma once

#include <stdint.h>

typedef struct {
	uint8_t* head;
	uint8_t* data;
	uint8_t* tail;
	uint8_t* end;
	uint32_t len;
} net_buf_t;

net_buf_t* net_buf_alloc(uint32_t size);
void net_buf_free(net_buf_t* buf);

void net_buf_reserve(net_buf_t* buf, uint32_t len);
void* net_buf_pull(net_buf_t* buf, uint32_t len);
void* net_buf_push(net_buf_t* buf, uint32_t len);

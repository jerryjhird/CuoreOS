// (jerryjhird notes) i hate this. replace ASAP

#include "netbuf.h"
#include "mem/heap.h"
#include <stdint.h>
#include "panic.h"

net_buf_t* net_buf_alloc(uint32_t size) {
	net_buf_t* buf = (net_buf_t*)malloc(sizeof(net_buf_t));
	if (!buf) return NULL;

	uint8_t* data = (uint8_t*)malloc(size + 64);
	if (!data) {
		free(buf);
		return NULL;
	}

	buf->head = data;
	buf->data = data + 64;
	buf->tail = data + 64;
	buf->end  = data + 64 + size;
	buf->len  = 0;

	return buf;
}

void net_buf_free(net_buf_t* buf) {
	if (!buf) return;
	if (buf->head) free(buf->head);
	free(buf);
}

void net_buf_reserve(net_buf_t* buf, uint32_t len) {
	buf->data += len;
	buf->tail += len;
}

void* net_buf_pull(net_buf_t* buf, uint32_t len) {
	void* prev_data = buf->data;
	buf->data += len;
	buf->len -= len;
	return prev_data;
}

void* net_buf_push(net_buf_t* buf, uint32_t len) {
	if (buf->data - len < buf->head) {
		panic("NETBUF", "headroom exhaustion");
	}
	buf->data -= len;
	buf->len += len;
	return buf->data;
}

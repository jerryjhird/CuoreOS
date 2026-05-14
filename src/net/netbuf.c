#include "netbuf.h"

#include "mem/heap.h"
#include "mem/dmalloc.h"
#include <stdint.h>
#include "panic.h"

#define HEADROOM 64

net_buf_t* net_buf_alloc(uint32_t size) {
	net_buf_t* buf = (net_buf_t*)malloc(sizeof(net_buf_t));
	dmalloc_ret_t res = dmalloc32(size + HEADROOM);

	if (!res.virt) {
		free(buf);
		return NULL;
	}

	buf->dm_handle = res;
	uint8_t* data = (uint8_t*)res.virt;
	buf->head = data;
	buf->data = data + HEADROOM;
	buf->tail = data + HEADROOM;
	buf->end  = data + HEADROOM + size;
	buf->len  = 0;

	return buf;
}

void net_buf_free(net_buf_t* buf) {
	if (buf->dm_handle.virt) {
		dmfree(buf->dm_handle.virt);
	}
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
		// TODO: instead of a panic implement resizing
		panic("NETBUF", "headroom exhaustion");
	}
	buf->data -= len;
	buf->len += len;
	return buf->data;
}

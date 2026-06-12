#pragma once

#include <stdint.h>
#include "net/netbuf.h"
#include "devices/types.h"

typedef struct __attribute__((packed)) {
	uint8_t  version_ihl;
	uint8_t  tos;
	uint16_t len;
	uint16_t id;
	uint16_t flags_offset;
	uint8_t  ttl;
	uint8_t  proto;
	uint16_t checksum;
	uint32_t src;
	uint32_t dest;
} ipv4_header_t;

void ipv4_handle(kernel_net_dev_t* dev, net_buf_t* buf);
void ipv4_send(kernel_net_dev_t* dev, net_buf_t* buf, uint32_t dest_ip, uint8_t proto);
uint16_t ipv4_checksum(void* vdata, size_t length);

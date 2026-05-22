#pragma once

#include <stdint.h>
#include "device/types.h"
#include "net/ip/ipv4.h"
#include "net/socket.h"

typedef struct {
	uint16_t src_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
} __attribute__((packed)) udp_header_t;

void udp_send(kernel_net_dev_t* dev, volatile socket_t* s, void* data, size_t len);
void udp_send_nosock(kernel_net_dev_t* dev, uint32_t dest_ip, uint16_t src, uint16_t dest, void* data, size_t len);
void udp_handle(kernel_net_dev_t* dev, ipv4_header_t* ip_hdr, void* transport_data, size_t len);

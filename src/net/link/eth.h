#pragma once

#include <stdint.h>
#include "devices.h"
#include "net/netbuf.h"

#define ETH_ADDR_LEN 6
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV4 0x0800

typedef struct __attribute__((packed)) {
	uint8_t  dest[ETH_ADDR_LEN];
	uint8_t  src[ETH_ADDR_LEN];
	uint16_t type; // BE
} eth_header_t;

void eth_init(kernel_net_dev_t* dev);
void eth_send(kernel_net_dev_t* dev, net_buf_t* buf, uint8_t* dest_mac, uint16_t type);
void eth_receive(kernel_net_dev_t* dev, void* data, size_t len);

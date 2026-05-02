#pragma once
#include <stdint.h>
#include "netbuf.h"
#include "eth.h"

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4 0x0800
#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY 2

typedef struct __attribute__((packed)) {
	uint16_t htype;
	uint16_t ptype;
	uint8_t  hlen;
	uint8_t  plen;
	uint16_t oper;
	uint8_t  sha[6];
	uint32_t spa;
	uint8_t  tha[6];
	uint32_t tpa;
} arp_packet_t;

typedef struct {
	uint32_t ip;
	uint8_t  mac[6];
	uint32_t last_updated;
	bool	 assigned;
} arp_entry_t;

#define ARP_CACHE_SIZE 256

extern arp_entry_t arp_cache[ARP_CACHE_SIZE];

uint8_t* arp_lookup(kernel_net_dev_t* dev, uint32_t ip);
uint8_t* arp_silent_lookup(uint32_t ip);

void arp_handle(kernel_net_dev_t* dev, net_buf_t* buf);
void arp_send_request(kernel_net_dev_t* dev, uint32_t target_ip);
void arp_send_reply(kernel_net_dev_t* dev, uint8_t* dest_mac, uint32_t dest_ip);

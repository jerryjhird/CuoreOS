#include "ipv4.h"

#include "icmp.h"
#include "net/link/eth.h"
#include "net/arp.h"
#include "net/transport/udp.h"
#include "mem/mem.h"
#include "logbuf.h"
#include <stddef.h>

uint16_t ipv4_checksum(void* vdata, size_t length) {
	uint32_t sum = 0;
	uint16_t* data = (uint16_t*)vdata;

	while (length > 1) {
		sum += *data++;
		length -= 2;
	}

	if (length > 0) sum += *(uint8_t*)data;

	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

	return (uint16_t)~sum;
}

void ipv4_send(kernel_net_dev_t* dev, net_buf_t* buf, uint32_t dest_ip, uint8_t proto) {
	ipv4_header_t* ip = (ipv4_header_t*)net_buf_push(buf, sizeof(ipv4_header_t));

	ip->version_ihl = (4 << 4) | 5;
	ip->tos = 0;
	ip->len = HTONS(buf->len);
	ip->id = HTONS(0);
	ip->flags_offset = 0;
	ip->ttl = 64;
	ip->proto = proto;
	ip->src = dev->ip_addr;
	ip->dest = dest_ip;
	ip->checksum = 0;
	ip->checksum = ipv4_checksum(ip, sizeof(ipv4_header_t));

	uint32_t next_hop_ip = dest_ip;

	if ((dest_ip & dev->subnet_mask) != (dev->ip_addr & dev->subnet_mask)) {
		next_hop_ip = dev->gateway;
	}

	uint8_t* dest_mac = arp_lookup(dev, next_hop_ip);

	if (dest_mac) {
		eth_send(dev, buf, dest_mac, ETH_TYPE_IPV4);
	}
}

void ipv4_handle(kernel_net_dev_t* dev, net_buf_t* buf) {
	if (buf->len < sizeof(ipv4_header_t)) return;

	ipv4_header_t* ip = (ipv4_header_t*)net_buf_pull(buf, sizeof(ipv4_header_t));

	if ((ip->version_ihl >> 4) != 4) return;
	if (ip->dest != dev->ip_addr && ip->dest != 0xFFFFFFFF) return;

	switch (ip->proto) {
		case 1: // ICMP
			icmp_handle(dev, buf, ip);
			break;
		case 17: // UDP
			udp_handle(dev, ip, buf->data, buf->len);
			break;
	}
}

#include "arp.h"
#include "mem/mem.h"

arp_entry_t arp_cache[ARP_CACHE_SIZE];

static void arp_cache_update(uint32_t ip, uint8_t* mac) {
	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].assigned && arp_cache[i].ip == ip) {
			memcpy(arp_cache[i].mac, mac, 6);
			return;
		}
	}

	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (!arp_cache[i].assigned) {
			arp_cache[i].ip = ip;
			memcpy(arp_cache[i].mac, mac, 6);
			arp_cache[i].assigned = true;
			return;
		}
	}
}

void arp_send_reply(kernel_net_dev_t* dev, uint8_t* dest_mac, uint32_t dest_ip) {
	net_buf_t* buf = net_buf_alloc(128);
	if (!buf) return;

	net_buf_reserve(buf, sizeof(eth_header_t));

	arp_packet_t* arp = (arp_packet_t*)net_buf_push(buf, sizeof(arp_packet_t));

	arp->htype = HTONS(ARP_HTYPE_ETHERNET);
	arp->ptype = HTONS(ARP_PTYPE_IPV4);
	arp->hlen = 6;
	arp->plen = 4;
	arp->oper = HTONS(ARP_OPER_REPLY);

	memcpy(arp->sha, dev->mac, 6);
	arp->spa = dev->ip_addr;
	memcpy(arp->tha, dest_mac, 6);
	arp->tpa = dest_ip;

	eth_send(dev, buf, dest_mac, ETH_TYPE_ARP);

	net_buf_free(buf);
}

void arp_send_request(kernel_net_dev_t* dev, uint32_t target_ip) {
	net_buf_t* buf = net_buf_alloc(128);
	if (!buf) return;

	net_buf_reserve(buf, sizeof(eth_header_t));
	arp_packet_t* arp = (arp_packet_t*)net_buf_push(buf, sizeof(arp_packet_t));

	arp->htype = HTONS(ARP_HTYPE_ETHERNET);
	arp->ptype = HTONS(ARP_PTYPE_IPV4);
	arp->hlen = 6;
	arp->plen = 4;
	arp->oper = HTONS(ARP_OPER_REQUEST);

	memcpy(arp->sha, dev->mac, 6);
	arp->spa = dev->ip_addr;

	memset(arp->tha, 0, 6);
	arp->tpa = target_ip;

	// broadcast MAC
	uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	eth_send(dev, buf, broadcast, ETH_TYPE_ARP);

	net_buf_free(buf);
}

uint8_t* arp_silent_lookup(uint32_t ip) {
	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].assigned && arp_cache[i].ip == ip) {
			return arp_cache[i].mac;
		}
	}
	return NULL;
}

uint8_t* arp_lookup(kernel_net_dev_t* dev, uint32_t ip) {
	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].assigned && arp_cache[i].ip == ip) {
			return arp_cache[i].mac;
		}
	}

	// not cached so find it
	arp_send_request(dev, ip);

	return NULL;
}

void arp_handle(kernel_net_dev_t* dev, net_buf_t* buf) {
	if (buf->len < sizeof(arp_packet_t)) return;

	arp_packet_t* arp = (arp_packet_t*)buf->data;
	uint16_t oper = NTOHS(arp->oper);

	arp_cache_update(arp->spa, arp->sha);

	if (oper == ARP_OPER_REQUEST) {
		if (arp->tpa == dev->ip_addr) {
			arp_send_reply(dev, arp->sha, arp->spa);
		}
	}
}
